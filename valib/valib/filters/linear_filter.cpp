#include "linear_filter.h"

static const int flush_none   = 0;
static const int flush_eos    = 1;
static const int flush_reinit = 2;

LinearFilter::LinearFilter()
: flushing(flush_none), size(0), out_size(0), buffered_samples(0)
{}

LinearFilter::~LinearFilter()
{}

///////////////////////////////////////////////////////////////////////////////
// Filter interface
///////////////////////////////////////////////////////////////////////////////

void
LinearFilter::reset()
{
  if (flushing & flush_reinit)
  {
    out_spk = in_spk;
    init(in_spk, out_spk);

    // Timing won't work correctly with SRC, because it's
    // hard to track the internal buffer size of the
    // filter in this case.
    assert(in_spk.sample_rate == out_spk.sample_rate);
  }
  flushing = flush_none;
  reset_state();

  samples.zero();
  size = 0;
  out_samples.zero();
  out_size = 0;
  buffered_samples = 0;
  sync_helper.reset();
}

bool
LinearFilter::is_ofdd() const
{
  return false;
}

bool
LinearFilter::query_input(Speakers spk) const
{ 
  if (!spk.is_linear() || spk.sample_rate == 0 || spk.mask == 0)
    return false;
  return query(spk);
}

bool
LinearFilter::set_input(Speakers spk)
{
  if (!query_input(spk))
  {
    reset();
    return false;
  }

  in_spk = spk;
  out_spk = spk;
  FILTER_SAFE(init(spk, out_spk));
  assert(in_spk.sample_rate == out_spk.sample_rate);
  reset();
  return true;
}

Speakers
LinearFilter::get_input() const
{
  return in_spk;
}

bool
LinearFilter::process(const Chunk *chunk)
{
  if (chunk->is_dummy())
    return true;

  if (in_spk != chunk->spk)
    FILTER_SAFE(set_input(chunk->spk));

  if (chunk->sync)
    sync(chunk->time);

  sync_helper.receive_sync(chunk, buffered_samples);
  samples  = chunk->samples;
  size     = chunk->size;

  if (chunk->eos)
    flushing |= flush_eos;

  out_size = 0;
  out_samples.zero();
  FILTER_SAFE(process());
  return true;
}

Speakers
LinearFilter::get_output() const
{
  return out_spk;
}

bool
LinearFilter::is_empty() const
{
  if (size > 0 || out_size > 0 || flushing != flush_none) return false;
  return true;
}

bool
LinearFilter::get_chunk(Chunk *chunk)
{
  if (size > 0 && out_size == 0)
    FILTER_SAFE(process());

  if (out_size)
  {
    chunk->set_linear(out_spk, out_samples, out_size);
    sync_helper.send_sync(chunk, 1.0 / in_spk.sample_rate);
    sync_helper.drop(out_size);
    out_size = 0;
    return true;
  }

  if (flushing != flush_none && need_flushing())
  {
    FILTER_SAFE(flush());
    chunk->set_linear(out_spk, out_samples, out_size);
    sync_helper.send_sync(chunk, 1.0 / in_spk.sample_rate);
    sync_helper.drop(out_size);
    out_size = 0;
    return true;
  }

  chunk->set_empty(out_spk);
  if (flushing != flush_none)
  {
    assert(buffered_samples == 0); // incorrect number of samples flushed
    if (flushing & flush_eos)
    {
      chunk->set_eos();
      reset();
    }
    else
    {
      Speakers old_out_spk = out_spk;
      out_spk = in_spk;
      init(in_spk, out_spk);
      reset_state();
      buffered_samples = 0;

      assert(old_out_spk == out_spk); // format change is not allowed
      assert(in_spk.sample_rate == out_spk.sample_rate); // sample rate conversion is not allowed
    }
    flushing = flush_none;
  }
  return true;
}

bool
LinearFilter::process()
{
  out_size = 0;
  while (size > 0 && out_size == 0)
  {
    size_t gone = 0;
    FILTER_SAFE(process_samples(samples, size, out_samples, out_size, gone));

    // detect endless loop
    assert(gone > 0 || out_size > 0);

    if (gone > size)
    {
      // incorrect number of samples processed
      assert(false);
      gone = size;
    }
    size -= gone;
    samples += gone;

    buffered_samples += gone;
    if (buffered_samples < out_size)
    {
      // incorrect number of output samples
      assert(false);
      buffered_samples = out_size;
    }
    buffered_samples -= out_size;
  }
  return true;
}

bool
LinearFilter::flush()
{
  out_size = 0;
  while (out_size == 0 && need_flushing())
  {
    FILTER_SAFE(flush(out_samples, out_size));

    // detect endless loop
    assert(out_size > 0 || !need_flushing());

    if (buffered_samples > out_size)
    {
      // incorrect number of samples flushed
      assert(false);
      buffered_samples = out_size;
    }
    buffered_samples -= out_size;
  }
  return true;
}

bool
LinearFilter::reinit(bool format_change)
{
  if (!need_flushing() && !format_change)
  {
    assert(buffered_samples == 0); // we should flush if we have buffered samples

    Speakers old_out_spk = out_spk;
    out_spk = in_spk;
    init(in_spk, out_spk);
    reset_state();
    buffered_samples = 0;

    assert(old_out_spk == out_spk); // format change is not allowed
    assert(in_spk.sample_rate == out_spk.sample_rate); // sample rate conversion is not allowed
  }
  else if (format_change)
    flushing |= flush_eos;
  else
    flushing |= flush_reinit;

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// LinearFilter interface
///////////////////////////////////////////////////////////////////////////////

bool
LinearFilter::query(Speakers spk) const
{
  return true;
}

bool
LinearFilter::init(Speakers spk, Speakers &out_spk)
{
  out_spk = spk;
  return true;
}

void
LinearFilter::reset_state()
{}

void
LinearFilter::sync(vtime_t time)
{}

bool
LinearFilter::process_samples(samples_t in, size_t in_size, samples_t &out, size_t &out_size, size_t &gone)
{
  out = in;
  out_size = in_size;
  gone = in_size;
  FILTER_SAFE(process_inplace(in, in_size));
  return true;
}

bool
LinearFilter::process_inplace(samples_t in, size_t in_size)
{
  return true;
}

bool
LinearFilter::flush(samples_t &out, size_t &out_size)
{
  out.zero();
  out_size = 0;
  return true;
}

bool
LinearFilter::need_flushing() const
{
  return false;
}
