/*
  ParserFilter
  Uses parser to transform the stream.
*/

#ifndef VALIB_PARSER_FILTER_H
#define VALIB_PARSER_FILTER_H

#include "../filter.h"
#include "../parser.h"
#include "../sync.h"

class ParserFilter : public NullFilter
{
protected:
  enum state_t 
  {
    state_trans,
    state_empty, 
    state_full, 
    state_no_frame, 
    state_format_change
  };

  FrameParser *parser;       // parser to use
  StreamBuffer stream;       // stream buffer
  SyncHelper   sync_helper;  // syncronization helper

  Speakers out_spk;          // output format
  state_t  state;            // filter state
  bool     new_stream;       // new stream found
  int      errors;           // number of parsing errors

  bool load_parse_frame();
  void send_frame(Chunk *chunk);
  void send_eos(Chunk *chunk);

public:
  ParserFilter();
  ParserFilter(FrameParser *parser);
  ~ParserFilter();

  /////////////////////////////////////////////////////////
  // Own interface

  bool set_parser(FrameParser *parser);
  const FrameParser *get_parser() const;

  int  get_frames() const { return stream.get_frames(); }
  int  get_errors() const { return errors; }

  size_t get_info(char *buf, size_t size) const;
  HeaderInfo header_info() const { return stream.header_info(); }

  /////////////////////////////////////////////////////////
  // Filter interface

  virtual void reset();
  virtual bool is_ofdd() const;

  virtual bool query_input(Speakers spk) const;
  virtual bool process(const Chunk *chunk);

  virtual Speakers get_output() const;
  virtual bool is_empty() const;
  virtual bool get_chunk(Chunk *chunk);
};

#endif
