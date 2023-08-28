/*
  Multiple header parser
*/

#ifndef VALIB_MULTI_HEADER_H
#define VALIB_MULTI_HEADER_H

#include "../parser.h"

class MultiHeader : public HeaderParser
{
protected:
  size_t f_header_size;
  size_t f_min_frame_size;
  size_t f_max_frame_size;

  const HeaderParser **parsers;
  size_t nparsers;

public:
  MultiHeader();
  MultiHeader(const FrameParser *const *parsers, size_t nparsers);
  MultiHeader(const HeaderParser *const *parsers, size_t nparsers);
  ~MultiHeader();

  bool set_parsers(const FrameParser *const *parsers, size_t nparsers);
  bool set_parsers(const HeaderParser *const *parsers, size_t nparsers);
  void release_parsers();

  /////////////////////////////////////////////////////////
  // HeaderParser overrides

  virtual size_t   header_size()    const { return f_header_size;    }
  virtual size_t   min_frame_size() const { return f_min_frame_size; }
  virtual size_t   max_frame_size() const { return f_max_frame_size; }
  virtual bool     can_parse(int format) const;

  virtual bool     parse_header(const uint8_t *hdr, HeaderInfo *hinfo = 0) const;
  virtual bool     compare_headers(const uint8_t *hdr1, const uint8_t *hdr2) const;
  virtual size_t   header_info(const uint8_t *hdr, char *buf, size_t size) const;
};

#endif
