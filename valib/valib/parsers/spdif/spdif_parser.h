/*
  SPDIF parser class
  Converts SPDIF stream to raw AC3/MPA/DTS stream.
*/

#ifndef VALIB_SPDIF_PARSER_H
#define VALIB_SPDIF_PARSER_H

#include "../../parser.h"

class SPDIFParser : public FrameParser
{
public:
  bool big_endian;

  SPDIFParser(bool big_endian);
  ~SPDIFParser();

  /////////////////////////////////////////////////////////
  // Own interface

  bool get_big_endian() const           { return big_endian;        }
  void set_big_endian(bool _big_endian) { big_endian = _big_endian; }

  HeaderInfo header_info() const        { return hdr; }

  /////////////////////////////////////////////////////////
  // FrameParser overrides

  virtual const HeaderParser *header_parser() const;

  virtual void reset();
  virtual bool parse_frame(uint8_t *frame, size_t size);

  virtual Speakers  get_spk()      const { return hdr.spk;      }
  virtual samples_t get_samples()  const { samples_t samples; samples.zero(); return samples; }
  virtual size_t    get_nsamples() const { return hdr.nsamples; }
  virtual uint8_t  *get_rawdata()  const { return data;         }
  virtual size_t    get_rawsize()  const { return data_size;    }

  virtual size_t stream_info(char *buf, size_t size) const;
  virtual size_t frame_info(char *buf, size_t size) const;

protected:
  uint8_t    *data;
  size_t      data_size;
  HeaderInfo  hdr;

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

#endif
