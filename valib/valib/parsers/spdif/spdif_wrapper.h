/*
  SPDIF wrapper class
  Converts raw AC3/MPA/DTS stream to SPDIF stream.

  Does raw stream output if the stream given cannot be spdifed (high-bitrate
  DTS for example). Can convert DTS stream type to 14/16 bits. Supports padded
  and wrapped SPDIF stream types. Can re-wrap SPDIF stream with conversion
  between SPDIF stream types.
*/

#ifndef VALIB_SPDIF_WRAPPER_H
#define VALIB_SPDIF_WRAPPER_H

#include "../../parser.h"
#include "../multi_header.h"
#include "spdif_parser.h"

#define DTS_MODE_AUTO    0
#define DTS_MODE_WRAPPED 1
#define DTS_MODE_PADDED  2

#define DTS_CONV_NONE    0
#define DTS_CONV_16BIT   1
#define DTS_CONV_14BIT   2

class SPDIFWrapper : public FrameParser
{
public:
  int  dts_mode;
  int  dts_conv;

  SPDIFWrapper(int dts_mode = DTS_MODE_AUTO, int dts_conv = DTS_CONV_NONE);
  ~SPDIFWrapper();

  HeaderInfo header_info() const { return hdr; }

  /////////////////////////////////////////////////////////
  // FrameParser overrides

  virtual const HeaderParser *header_parser() const;

  virtual void reset();
  virtual bool parse_frame(uint8_t *frame, size_t size);

  virtual Speakers  get_spk()      const { return spk;          }
  virtual samples_t get_samples()  const { samples_t samples; samples.zero(); return samples; }
  virtual size_t    get_nsamples() const { return hdr.nsamples; }
  virtual uint8_t  *get_rawdata()  const { return spdif_frame;  }
  virtual size_t    get_rawsize()  const { return spdif_size;   }

  virtual size_t stream_info(char *buf, size_t size) const;
  virtual size_t frame_info(char *buf, size_t size) const;

protected:
  uint8_t    *buf;          // output frame buffer

  MultiHeader spdifable;    // spdifable formats header parser
  SPDIFParser spdif_parser; // required to rewrap spdif input
  HeaderInfo  hdr;          // input raw frame info

  Speakers    spk;          // output format
  uint8_t    *spdif_frame;  // spdif frame pointer
  size_t      spdif_size;   // spdif frame size

  bool use_header;          // use SPDIF header
  int spdif_bs;             // SPDIF bitstream type
  
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

    inline void set(uint16_t _type, uint16_t _len_bits)
    {
      zero1 = 0;
      zero2 = 0;
      zero3 = 0;
      zero4 = 0;

      sync1 = 0xf872;
      sync2 = 0x4e1f;
      type  = _type;
      len   = _len_bits;
    }
  };

};

#endif
