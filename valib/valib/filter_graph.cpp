#include "filter_graph.h"
#include "stdio.h"  // snprint
#include "string.h" // strdup

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

inline static bool is_invalid(int node)
{
  return (node != node_end) && (node == node_err || node < 0 || node > graph_nodes);
}


FilterGraph::FilterGraph(int _format_mask)
:start(_format_mask), end(-1)
{
  filter[node_start] = &start;
  filter[node_end]   = &end;
  drop_chain();
};

FilterGraph::~FilterGraph()
{};

///////////////////////////////////////////////////////////////////////////////
// Chain operatoins
///////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// Build filter chain after the specified node
//
// bool build_chain(int node)
// node - existing node after which we must build the chain
// updates nothing (uses add_node() to update the chain)

bool
FilterGraph::build_chain(int node)
{
  Speakers spk;
  while (node != node_end)
  {
    spk = filter[node]->get_output();
    FILTER_SAFE(add_node(node, spk));
    node = next[node];
  }

  return true;
}

/////////////////////////////////////////////////////////
// Drop filter chain and put graph into initial state
//
// void drop_chain()
// updates filter lists (forward and reverse)
// updates input/output formats and ofdd status

void
FilterGraph::drop_chain()
{
  ofdd = false;

  start.reset();
  end.reset();
  next[node_start] = node_end;
  prev[node_start] = node_end;
  next[node_end] = node_start;
  prev[node_end] = node_start;
  node_state[node_start] = ns_dirty;
  node_state[node_end] = ns_ok;
}

/////////////////////////////////////////////////////////
// Mark nodes dirty
//
// set_dirty() marks node as dirty when get_next() result
// for this node may change. In this case process_internal()
// will call get_next() for this node again and rebuild
// the chain if nessesary.
//
// invalidate_chain() marks all nodes as dirty to re-check
// all chain.
//
// Note that start node can be set dirty and end node 
// cannot. It is because get_next() is never called for 
// the end node but always called for the start node.

void
FilterGraph::set_dirty(int node)
{
  if (node == node_end || node < 0 || node > graph_nodes)
    return;

  if (node_state[node] == ns_ok)
    node_state[node] = ns_dirty;
}

void 
FilterGraph::invalidate_chain()
{
  int node = node_start;
  while (node != node_end)
  {
    if (node_state[node] == ns_ok)
      node_state[node] = ns_dirty;
    node = next[node];
  }
}

/////////////////////////////////////////////////////////
// Mark node to rebuild
//
// It is nessesary when graph parameters change and some
// nodes are required to be reinitialized. In his case
// process_internal() will flush this node (and therefore
// downstream nodes will be flushed too) and then remove
// it and rebuild the chain starting from this node.
//
// Note that we can mark end node to rebuild (in this
// case flushing will be sent to output) but cannot mark
// start node.

void 
FilterGraph::rebuild_node(int node)
{
  if (node == node_start || node < 0 || node > graph_nodes)
    return;

  node_state[node] = ns_flush;
}

/////////////////////////////////////////////////////////
// Add new node into the chain
// 
// bool add_node(int node, Speakers spk)
// node - parent node
// spk - input format for a new node
//
// updates filter lists (forward and reverse)
// updates output format and ofdd status

bool
FilterGraph::add_node(int node, Speakers spk)
{
  ///////////////////////////////////////////////////////
  // if ofdd filter is in transition state then drop 
  // the rest of the chain and set output format to 
  // spk_unknown

  if (spk == spk_unknown)
  {
    ofdd = true;

    end.set_input(spk);
    next[node] = node_end;
    prev[node_end] = node;
    return true;
  }

  ///////////////////////////////////////////////////////
  // find the next node

  int next_node = get_next(node, spk);

  // runtime protection
  // we may check get_next() result only here because
  // in all other cases wrong get_next() result forces
  // chain to rebuild and we'll get here anyway

  if (is_invalid(next_node))
    return false;

  ///////////////////////////////////////////////////////
  // end of the filter chain
  // drop the rest of the chain and 
  // set output format

  if (next_node == node_end)
  {
    end.set_input(spk);
    next[node] = node_end;
    prev[node_end] = node;
    return true;
  }

  ///////////////////////////////////////////////////////
  // build new filter into the filter chain

  // create filter

  filter[next_node] = init_filter(next_node, spk);

  // runtime protection
  // must do it BEFORE updating of filter lists
  // otherwise filter list may be broken in case of failure

  if (!filter[next_node])
    return false;

  // init filter
  // must do it BEFORE updating of filter lists
  // otherwise filter list may be broken in case of failure

  FILTER_SAFE(filter[next_node]->set_input(spk));

  // update filter lists
  next[node] = next_node;
  prev[next_node] = node;
  next[next_node] = node_end;
  prev[node_end] = next_node;
  node_state[next_node] = ns_ok;

  // update ofdd status
  // aggregate is data-dependent if chain has
  // at least one ofdd filter

  ofdd = false;
  node = next[node_start];
  while (node != node_end)
  {
    ofdd |= filter[node]->is_ofdd();
    node = next[node];
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Chain data flow
///////////////////////////////////////////////////////////////////////////////

bool
FilterGraph::process_internal(bool rebuild)
{
  Speakers spk;
  Chunk chunk;

  int node = prev[node_end];
  while (node != node_end)
  {
    /////////////////////////////////////////////////////
    // find full filter

    if (filter[node]->is_empty())
    {
      // we need more data from upstream.
      node = prev[node];
      continue;
    }

    /////////////////////////////////////////////////////
    // filter is full so get_output() must always
    // report format of next output chunk

    spk = filter[node]->get_output();

    /////////////////////////////////////////////////////
    // Rebuild the filter chain

    if (spk != filter[next[node]]->get_input())
    {
      // Rebuild the chain according to format changes
      // during normal data flow.
      //
      // If format was changed it means that flushing was
      // send before (see format change rules) and we may
      // rebuild the chain right now
      FILTER_SAFE(build_chain(node));
    }
    else if (node_state[next[node]] == ns_rebuild)
    {
      // Rebuild the chain after flushing.
      // We have flushed downstream and may rebuild it now.
      FILTER_SAFE(build_chain(node));
    }
    else if (rebuild && node_state[node] == ns_dirty)
    {
      // We should rebuild graph according to changes in 
      // get_next() call ONLY when we do it from the top
      // of the chain, i.e. 'rebuild' flag should be set
      // in process() and clear in get_chunk() call.
      // (otherwise partially changed graph is possible)
      //
      // If chain changes without format change we must
      // flush downstream before rebuilding the chain.
      if (next[node] != get_next(node, spk))
        node_state[next[node]] = ns_flush;
      else
        node_state[node] = ns_ok;
    }

    /////////////////////////////////////////////////////
    // process data downstream

    if (node_state[next[node]] == ns_flush)
    {
      // flush downstream
      chunk.set_empty(spk, false, 0, true);
      FILTER_SAFE(filter[next[node]]->process(&chunk));
      node_state[next[node]] = ns_rebuild;
    }
    else
    {
      // process data
      FILTER_SAFE(filter[node]->get_chunk(&chunk));
      FILTER_SAFE(filter[next[node]]->process(&chunk));
    }
    node = next[node];
  }

  return true;
}

/////////////////////////////////////////////////////////
// Print chain
//
// int chain_text(char *buf, size_t buf_size)
// buf - output buffer
// buf_size - output buffer size
// returns number of printed bytes

size_t
FilterGraph::chain_text(char *buf, size_t buf_size) const
{
  //XILASZ modified to be more readable
  size_t i;
  Speakers spk;

  char *buf_ptr = buf;
  int node = next[node_start];

  spk = filter[node]->get_input();

  if (spk.mask || spk.sample_rate)
    i = snprintf(buf_ptr, buf_size, "(%s %s %i)", spk.format_text(), spk.mode_text(), spk.sample_rate);
  else
    i = snprintf(buf_ptr, buf_size, "(%s)", spk.format_text());

  buf_ptr += i;
  buf_size = (buf_size > i)? buf_size - i: 0;

  while (node != node_end)
  {
    spk = filter[node]->get_output();

    if (spk.mask || spk.sample_rate)
      i = snprintf(buf_ptr, buf_size, "\n- %s\n- (%s %s %i)", get_name(node), spk.format_text(), spk.mode_text(), spk.sample_rate);
    else
      i = snprintf(buf_ptr, buf_size, "\n- %s\n- (%s)", get_name(node), spk.format_text());
    buf_ptr += i;
    buf_size = (buf_size > i)? buf_size - i: 0;

    node = next[node];
  }
  return buf_ptr - buf;
}

///////////////////////////////////////////////////////////////////////////////
// Filter interface
///////////////////////////////////////////////////////////////////////////////

void
FilterGraph::reset()
{
  drop_chain();
}

bool
FilterGraph::is_ofdd() const
{
  return ofdd;
}

bool
FilterGraph::query_input(Speakers _spk) const
{
  // format mask based test
  if (!start.query_input(_spk))
    return false;

  // get_next() test
  return !is_invalid(get_next(node_start, _spk));
}

bool
FilterGraph::set_input(Speakers _spk)
{
  drop_chain();
  if (!query_input(_spk))
    return false;

  FILTER_SAFE(start.set_input(_spk));
  FILTER_SAFE(build_chain(node_start));
  return true;
}

Speakers
FilterGraph::get_input() const
{
  return start.get_input();
};

bool
FilterGraph::process(const Chunk *_chunk)
{
  if (_chunk->is_dummy())
    return true;

  if (_chunk->spk != filter[next[node_start]]->get_input())
    FILTER_SAFE(set_input(_chunk->spk));

  FILTER_SAFE(start.process(_chunk))
  FILTER_SAFE(process_internal(true));
  return true;
};

Speakers
FilterGraph::get_output() const
{
  return end.get_output();
};

bool
FilterGraph::is_empty() const
{
  ///////////////////////////////////////////////////////
  // graph is not empty if there're at least one 
  // non-empty filter

  int node = node_end;
  do {
    if (!filter[node]->is_empty())
      return false;

    node = prev[node];
  } while (node != node_end);
  return true;
}

bool
FilterGraph::get_chunk(Chunk *chunk)
{
  ///////////////////////////////////////////////////////
  // if there're something to output from the last filter
  // get it...

  if (!end.is_empty())
    return end.get_chunk(chunk);

  ///////////////////////////////////////////////////////
  // if the last filter is empty then do internal data
  // processing and try to get output afterwards

  FILTER_SAFE(process_internal(false));
  if (!end.is_empty())
    return end.get_chunk(chunk);

  ///////////////////////////////////////////////////////
  // return dummy chunk

  chunk->set_dummy();

  return true;
}












///////////////////////////////////////////////////////////////////////////////
// FilterChain
///////////////////////////////////////////////////////////////////////////////

FilterChain::FilterChain(int _format_mask)
:FilterGraph(_format_mask)
{
  chain_size = 0;
};

FilterChain::~FilterChain()
{
  drop();
};

///////////////////////////////////////////////////////////////////////////////
// FilterChain interface
///////////////////////////////////////////////////////////////////////////////

bool
FilterChain::add_front(Filter *_filter, const char *_desc)
{
  if (chain_size >= graph_nodes)
    return false;

  for (int i = chain_size; i > 0; i--)
  {
    chain[i] = chain[i-1];
    desc[i] = desc[i-1];
  }

  chain[0] = _filter;
  desc[0] = _strdup(_desc);
  chain_size++;
  return true;
}

bool
FilterChain::add_back(Filter *_filter, const char *_desc)
{
  if (chain_size >= graph_nodes)
    return false;

  chain[chain_size] = _filter;
  desc[chain_size] = _strdup(_desc);
  chain_size++;
  return true;
}

void
FilterChain::drop()
{
  drop_chain();
  for (int i = 0; i < chain_size; i++)
    safe_delete(desc[i]);
  chain_size = 0;
}

///////////////////////////////////////////////////////////////////////////////
// FilterGraph overrides
///////////////////////////////////////////////////////////////////////////////

const char *
FilterChain::get_name(int _node) const
{
  if (_node >= chain_size)
    return 0;

  return desc[_node];
}

Filter *
FilterChain::init_filter(int _node, Speakers _spk)
{
  if (_node >= chain_size)
    return 0;

  return chain[_node];
}

int
FilterChain::get_next(int _node, Speakers _spk) const
{
  if (_node == node_start)
    return chain_size? 0: node_end;

  if (_node >= chain_size - 1)
    return node_end;

  if (chain[_node+1]->query_input(_spk))
    return _node + 1;
  else
    return node_err;
}
