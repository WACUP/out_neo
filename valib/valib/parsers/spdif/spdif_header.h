#ifndef VALIB_SPDIF_HEADER_H
#define VALIB_SPDIF_HEADER_H

#include "../../parser.h"

class SPDIFHeader : public HeaderParser
{
public:
  SPDIFHeader() {};

  /////////////////////////////////////////////////////////
  // HeaderParser overrides
  // Max frame size for SPDIF 8192
  // Header size, min and max frame sizes are determined
  // by the number of samples the frame contains:
  //
  //       header   min    max
  // ------------------------
  // MPA:       4   384    1152
  // AC3:       8  1536    1536
  // DTS:      16   192    4096 (only 2048 is supported by SPDIF)
  //
  // Header size = SPDIF header size + max header size = 32
  // Minimum SPDIF frame size = 192 * 4 = 768
  // Maximum SPDIF frame size = 2048 * 4 = 8192

  virtual size_t   header_size()    const  { return 32;   }
  virtual size_t   min_frame_size() const  { return 768;  }
  virtual size_t   max_frame_size() const  { return 8192; }
  virtual bool     can_parse(int format) const { return format == FORMAT_SPDIF; };

  virtual bool     parse_header(const uint8_t *hdr, HeaderInfo *hinfo = 0) const;
  virtual bool     compare_headers(const uint8_t *hdr1, const uint8_t *hdr2) const;

  virtual size_t   header_info(const uint8_t *hdr, char *buf, size_t size) const;

protected:
  struct spdif_header_s
  {
    uint16_t zero1;
    uint16_t zero2;
    uint16_t zero3;
    uint16_t zero4;

    uint16_t sync1;   // Pa sync word 1
    uint16_t sync2;   // Pb sync word 2
    uint16_t type;    // Pc data type
    uint16_t len;     // Pd length-code (bits)
  };

};

extern const SPDIFHeader spdif_header;

#endif
