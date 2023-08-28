#ifndef VALIB_AC3_HEADER_H
#define VALIB_AC3_HEADER_H

#include "../../parser.h"

class AC3Header : public HeaderParser
{
public:
  AC3Header() {};

  /////////////////////////////////////////////////////////
  // HeaderParser overrides

  virtual size_t   header_size()    const  { return 8;    }
  virtual size_t   min_frame_size() const  { return 128;  }
  virtual size_t   max_frame_size() const  { return 3814; }
  virtual bool     can_parse(int format) const { return format == FORMAT_AC3; };

  virtual bool     parse_header(const uint8_t *hdr, HeaderInfo *hinfo = 0) const;
  virtual bool     compare_headers(const uint8_t *hdr1, const uint8_t *hdr2) const;
};

extern const AC3Header ac3_header;

#endif
