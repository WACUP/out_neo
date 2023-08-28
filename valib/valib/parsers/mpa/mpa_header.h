#ifndef VALIB_MPA_HEADER_H
#define VALIB_MPA_HEADER_H

#include "../../parser.h"

class MPAHeader : public HeaderParser
{
public:
  MPAHeader() {};

  /////////////////////////////////////////////////////////
  // HeaderParser overrides

  virtual size_t   header_size()    const  { return 4;    }
  virtual size_t   min_frame_size() const  { return 32;   }
  virtual size_t   max_frame_size() const  { return 1728; }
  virtual bool     can_parse(int format) const { return format == FORMAT_MPA; };

  virtual bool     parse_header(const uint8_t *hdr, HeaderInfo *hinfo = 0) const;
  virtual bool     compare_headers(const uint8_t *hdr1, const uint8_t *hdr2) const;
};

extern const MPAHeader mpa_header;

#endif
