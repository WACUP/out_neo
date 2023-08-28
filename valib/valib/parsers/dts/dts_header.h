#ifndef VALIB_DTS_HEADER_H
#define VALIB_DTS_HEADER_H

#include "../../parser.h"

class DTSHeader : public HeaderParser
{
public:
  DTSHeader() {};

  /////////////////////////////////////////////////////////
  // HeaderParser overrides

  virtual size_t   header_size()    const  { return 16;    }
  virtual size_t   min_frame_size() const  { return 96;    }
  virtual size_t   max_frame_size() const  { return 16384; }
  virtual bool     can_parse(int format) const { return format == FORMAT_DTS; };

  virtual bool     parse_header(const uint8_t *hdr, HeaderInfo *h = 0) const;
  virtual bool     compare_headers(const uint8_t *hdr1, const uint8_t *hdr2) const;
};

extern const DTSHeader dts_header;

#endif
