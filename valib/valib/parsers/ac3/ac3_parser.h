#ifndef VALIB_AC3_PARSER_H
#define VALIB_AC3_PARSER_H

#include "../../buffer.h"
#include "../../parser.h"
#include "../../bitstream.h"
#include "ac3_defs.h"
#include "ac3_imdct.h"

// todo: decode_block() for per-block decode

class AC3Info
{
public:
  int       bitrate;     // bitrate     (derived from frame_size & halfrate)
  int       halfrate;    // halfrate    (derived from bsid)

  int       fscod;       // sample rate code
  int       frmsizecod;  // frame size code

  int       bsid;        // bitstream identification
  int       bsmod;       // bitestream mode

  int       acmod;       // audio coding mode
  int       dsurmod;     // dolby surround mode
  bool      lfeon;       // lfe channel present

  sample_t  clev;        // center mix level
  sample_t  slev;        // surround mix level

  bool      compre;      // compression gain word exists
  sample_t  compr;       // compression gain word
  bool      compr2e;     // compression gain word 2 exists
  sample_t  compr2;      // compression gain word 2

  int       dialnorm;    // dialog normalization 
  int       dialnorm2;   // dialog normalization 2

  bool      langcode;    // language code exists
  int       langcod;     // language code
  bool      langcod2e;   // language code 2 exists
  int       langcod2;    // language code 2

  bool      audprodie;   // audio production information exists
  int       mixlevel;    // mixing level (SPL)
  int       roomtyp;     // room type
                        
  bool      audprodi2e;  // audio production information 2 exists
  int       mixlevel2;   // mixing level (SPL) 2
  int       roomtyp2;    // room type 2

  bool      copyrightb;  // copyright bit
  bool      origbs;      // original bitstream

  struct {
    int hours;
    int mins;
    int secs;
    int frames;
    int fracs;
  } timecode;            // timecode
};

class AC3FrameState
{
public:
  // general 
  bool blksw[5];         // non-reusable between blocks
  bool dithflag[5];      // non-reusable between blocks

  sample_t dynrng;       // needs reset at new frame
  sample_t dynrng2;      // needs reset at new frame

  // coupling info

  bool cplinu;
  bool chincpl[5];
  bool phsflginu;
  int  cplstrtmant;
  int  cplendmant;
  int  ncplbnd;
  int  cplbnd[18];
  sample_t cplco[5][18];

  // channel info

  int rematflg;
  int endmant[5];

  // exponents

  int8_t cplexps[256];
  int8_t exps[5][256];
  int8_t gainrng[5];     // non-reusable between blocks
  int8_t lfeexps[7];

  // bit allocation info

  int sdecay;
  int fdecay;
  int sgain;
  int dbknee;
  int floor;

  int cplsnroffset;
  int snroffset[5];
  int lfesnroffset;

  int cplfgain;
  int fgain[5];
  int lfefgain;

  int cplfleak;
  int cplsleak;

  // delta bit allocation

  int  cpldeltbae;       // needs reset at new frame
  int  deltbae[5];       // needs reset at new frame

  int8_t cpldeltba[50];
  int8_t deltba[5][50];

  // bit allocation

  int8_t cplbap[256];
  int8_t bap[5][256];
  int8_t lfebap[7];
};

class AC3Parser : public FrameParser, public AC3Info, public AC3FrameState
{
public:
  bool do_crc;        // do crc check
  bool do_dither;     // do dithering
  bool do_imdct;      // do IMDCT

  int frames;
  int errors;

public:
  AC3Parser();
  ~AC3Parser();

  /////////////////////////////////////////////////////////
  // FrameParser overrides

  virtual const HeaderParser *header_parser() const;

  virtual void reset();
  virtual bool parse_frame(uint8_t *frame, size_t size);

  virtual Speakers  get_spk()      const { return spk;               }
  virtual samples_t get_samples()  const { return samples;           }
  virtual size_t    get_nsamples() const { return AC3_FRAME_SAMPLES; }
  virtual uint8_t  *get_rawdata()  const { return 0;                 }
  virtual size_t    get_rawsize()  const { return 0;                 }

  virtual size_t stream_info(char *buf, size_t size) const;
  virtual size_t frame_info(char *buf, size_t size) const;

#ifndef AC3_DEBUG 
protected:
#else
public:
#endif

  /////////////////////////////////////////////////////////
  // AC3 parse

  Speakers  spk;        // output format
  uint8_t  *frame;      // frame data
  size_t    frame_size; // frame size
  int       bs_type;    // bitstream type

  SampleBuf samples;    // samples buffer
  SampleBuf delay;      // delayed samples buffer
  IMDCT     imdct;      // IMDCT
  ReadBS    bs;         // Bitstream reader

  int block;

  bool start_parse(uint8_t *frame, size_t size);
  bool crc_check();
  bool decode_block();
  bool parse_header();
  bool parse_block();
  bool parse_exponents(int8_t *exps, int8_t absexp, int expstr, int nexpgrps);
  bool parse_deltba(int8_t *deltba);
  void parse_coeff(samples_t samples);

};

#endif
