/*
  MPEG Audio parser
  MPEG1/2 LayerI and LayerII audio parser class
*/

#ifndef VALIB_MPA_PARSER_H
#define VALIB_MPA_PARSER_H

#include "../../parser.h"
#include "../../bitstream.h"
#include "../../buffer.h"
#include "mpa_defs.h"
#include "mpa_synth.h"


class MPAParser : public FrameParser
{
public:
  // raw frame header struct
  union Header
  {
    Header() {};
    Header(uint32_t i) { raw = i; }

    uint32_t raw;
    struct
    {
      unsigned emphasis           : 2;
      unsigned original           : 1;
      unsigned copyright          : 1;
      unsigned mode_ext           : 2;
      unsigned mode               : 2;
      unsigned extension          : 1;
      unsigned padding            : 1;
      unsigned sampling_frequency : 2;
      unsigned bitrate_index      : 4;
      unsigned error_protection   : 1;
      unsigned layer              : 2;
      unsigned version            : 1;
      unsigned sync               : 12;
    };
  };
  // bitstream information struct
  struct BSI
  {
    int bs_type;    // bitstream type
    int ver;        // stream version: 0 = MPEG1; 1 = MPEG2 LSF
    int layer;      // indicate layer (MPA_LAYER_X constants)
    int mode;       // channel mode (MPA_MODE_X constants)
    int bitrate;    // bitrate [bps]
    int freq;       // sampling frequency [Hz]

    int nch;        // number of channels
    int nsamples;   // number of samples in sample buffer (384 for Layer1, 1152 for LayerII)
    size_t frame_size; // frame size in bytes (not slots!)

    int jsbound;    // first band of joint stereo coding 
    int sblimit;    // total number of subbands 
  };

  Header   hdr; // raw header
  BSI      bsi; // bitstream information

  MPAParser();
  ~MPAParser();

  /////////////////////////////////////////////////////////
  // FrameParser overrides

  virtual const HeaderParser *header_parser() const;

  virtual void reset();
  virtual bool parse_frame(uint8_t *frame, size_t size);

  virtual Speakers  get_spk()      const { return spk;          }
  virtual samples_t get_samples()  const { return samples;      }
  virtual size_t    get_nsamples() const { return bsi.nsamples; }
  virtual uint8_t  *get_rawdata()  const { return 0;            }
  virtual size_t    get_rawsize()  const { return 0;            }

  virtual size_t stream_info(char *buf, size_t size) const;
  virtual size_t frame_info(char *buf, size_t size) const;

private:
  Speakers  spk;        // output format
  SampleBuf samples;    // samples buffer
  ReadBS    bs;         // bitstream reader

  SynthBuffer *synth[MPA_NCH]; // synthesis buffers
  int II_table;         // Layer II allocation table number 

  /////////////////////////////////////////////////////////
  // Common decoding functions

  bool parse_header(const uint8_t *frame, size_t size);
  bool crc_check(const uint8_t *frame, size_t protected_data_bits) const;

  /////////////////////////////////////////////////////////
  // Layer I

  bool I_decode_frame(const uint8_t *frame);
  void I_decode_fraction(
         sample_t *fraction[MPA_NCH], // pointers to sample_t[SBLIMIT] arrays
         int16_t  bit_alloc[MPA_NCH][SBLIMIT],
         sample_t scale[MPA_NCH][SBLIMIT]);

  /////////////////////////////////////////////////////////
  // Layer II

  bool II_decode_frame(const uint8_t *frame);
  void II_decode_fraction(
         sample_t *fraction[MPA_NCH], // pointers to sample_t[SBLIMIT*3] arrays
         int16_t  bit_alloc[MPA_NCH][SBLIMIT],
         sample_t scale[MPA_NCH][3][SBLIMIT],
         int x);
};

#endif
