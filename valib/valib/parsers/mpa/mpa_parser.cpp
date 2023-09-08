#include <stdio.h>
#include <string.h>
#include "../../crc.h"
#include "mpa_parser.h"
#include "mpa_header.h"
#include "mpa_tables.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


MPAParser::MPAParser()
{
  samples.allocate(2, MPA_NSAMPLES);
  synth[0] = new SynthBufferFPU();
  synth[1] = new SynthBufferFPU();

  // always useful
  reset();
}


MPAParser::~MPAParser()
{
  safe_delete(synth[0]);
  safe_delete(synth[1]);
}

///////////////////////////////////////////////////////////////////////////////
// FrameParser overrides

const HeaderParser *
MPAParser::header_parser() const
{
  return &mpa_header;
}

void 
MPAParser::reset()
{
  spk = spk_unknown;
  samples.zero();
  if (synth[0]) synth[0]->reset();
  if (synth[1]) synth[1]->reset();
}

bool
MPAParser::parse_frame(uint8_t *frame, size_t size)
{
  if (!parse_header(frame, size))
    return false;

  // convert bitstream format
  if (bsi.bs_type != BITSTREAM_8)
    if (bs_convert(frame, size, bsi.bs_type, frame, BITSTREAM_8) == 0)
      return false;

  bs.set(frame, 0, size * 8);
  bs.get(32); // skip header
  if (hdr.error_protection)
    bs.get(16); // skip crc

  switch (bsi.layer)
  {
    case MPA_LAYER_I:
      I_decode_frame(frame);
      break;

    case MPA_LAYER_II:
      II_decode_frame(frame);
      break;
  }

  return true;
}

bool
MPAParser::crc_check(const uint8_t *frame, size_t protected_data_bits) const
{
  if (!hdr.error_protection)
    return true;

  /////////////////////////////////////////////////////////
  // CRC check
  // Note that we include CRC word into processing AFTER
  // protected data. Due to CRC properties we must get
  // zero result in case of no errors.

  uint32_t crc = crc16.crc_init(0xffff);
  crc = crc16.calc_bits(crc, frame + 2, 0, 16); // header
  crc = crc16.calc_bits(crc, frame + 6, 0, protected_data_bits); // frame data
  crc = crc16.calc_bits(crc, frame + 4, 0, 16); // crc
  return crc == 0;
}


size_t
MPAParser::stream_info(char *buf, size_t size) const 
{
  char info[1024];

  size_t len = sprintf(info, 
    "MPEG Audio\n"
    "speakers: %s\n"
    "ver: %s\n"
    "frame size: %zi bytes\n"
    "stream: %s\n"
    "bitrate: %ikbps\n"
    "sample rate: %iHz\n"
    "bandwidth: %ikHz/%ikHz\n\0",
    spk.mode_text(),
    bsi.ver? "MPEG2 LSF": "MPEG1", 
    bsi.frame_size,
    (bsi.bs_type == BITSTREAM_8? "8 bit": "16bit low endian"), 
    bsi.bitrate / 1000,
    bsi.freq,
    bsi.jsbound * bsi.freq / SBLIMIT / 1000 / 2,
    bsi.sblimit * bsi.freq / SBLIMIT / 1000 / 2);

  if (len + 1 > size) len = size - 1;
  memcpy(buf, info, len + 1);
  buf[len] = 0;
  return len;
}

size_t
MPAParser::frame_info(char *buf, size_t size) const 
{
  if (buf && size) buf[0] = 0;
  return 0;
}

//////////////////////////////////////////////////////////////////////
// MPA parsing
//////////////////////////////////////////////////////////////////////

bool 
MPAParser::parse_header(const uint8_t *frame, size_t size)
{
  // 8 bit or 16 bit big endian stream sync
  if ((frame[0] == 0xff)         && // sync
     ((frame[1] & 0xf0) == 0xf0) && // sync
     ((frame[1] & 0x06) != 0x00) && // layer
     ((frame[2] & 0xf0) != 0xf0) && // bitrate
     ((frame[2] & 0x0c) != 0x0c))   // sample rate
  {
    uint32_t h = *(uint32_t *)frame;
    hdr = swab_u32(h);
    bsi.bs_type = BITSTREAM_8;
  }
  else
  // 16 bit low endian stream sync
  if ((frame[1] == 0xff)         && // sync
     ((frame[0] & 0xf0) == 0xf0) && // sync
     ((frame[0] & 0x06) != 0x00) && // layer
     ((frame[3] & 0xf0) != 0xf0) && // bitrate
     ((frame[3] & 0x0c) != 0x0c))   // sample rate
  {
    uint32_t h = *(uint32_t *)frame;
    hdr = (h >> 16) | (h << 16);
    bsi.bs_type = BITSTREAM_16LE;
  }
  else
    return false;

  hdr.error_protection = ~hdr.error_protection;

  // common information
  bsi.ver       = 1 - hdr.version;
  bsi.mode      = hdr.mode;
  bsi.layer     = 3 - hdr.layer;
  bsi.bitrate   = bitrate_tbl[bsi.ver][bsi.layer][hdr.bitrate_index] * 1000;
  bsi.freq      = freq_tbl[bsi.ver][hdr.sampling_frequency];

  bsi.nch       = bsi.mode == MPA_MODE_SINGLE? 1: 2;
  bsi.nsamples  = bsi.layer == MPA_LAYER_I? SCALE_BLOCK * SBLIMIT: SCALE_BLOCK * SBLIMIT * 3;

  // frame size calculation
  if (bsi.bitrate)
  {
    bsi.frame_size = bsi.bitrate * slots_tbl[bsi.layer] / bsi.freq + hdr.padding;
    if (bsi.layer == MPA_LAYER_I) 
      bsi.frame_size *= 4;

    if (bsi.frame_size > size)
      return false;
  }
  else
    bsi.frame_size = 0;

  // layerII: table select
  II_table = 0;
  if (bsi.layer == MPA_LAYER_II)
  {
    // todo: check for allowed bitrate ??? (look at sec 2.4.2.3 of ISO 11172-3)
    if (bsi.ver)
      // MPEG2 LSF
      II_table = 4; 
    else
    {
      // MPEG1
      if (bsi.mode == MPA_MODE_SINGLE)
        II_table = II_table_tbl[hdr.sampling_frequency][hdr.bitrate_index];
      else
        II_table = II_table_tbl[hdr.sampling_frequency][II_half_bitrate_tbl[hdr.bitrate_index]];
    }
  }

  // subband information
  bsi.sblimit = bsi.layer == MPA_LAYER_II?
                  II_sblimit_tbl[II_table]:
                  SBLIMIT;

  bsi.jsbound = bsi.mode == MPA_MODE_JOINT? 
                  jsbound_tbl[bsi.layer][hdr.mode_ext]: 
                  bsi.sblimit;

  spk = Speakers(FORMAT_LINEAR, (bsi.mode == MPA_MODE_SINGLE)? MODE_MONO: MODE_STEREO, bsi.freq);
  return true; 
}


///////////////////////////////////////////////////////////////////////////////
//  Layer II
///////////////////////////////////////////////////////////////////////////////


bool 
MPAParser::II_decode_frame(const uint8_t *frame)
{
  int sb, ch;
  int nch     = bsi.nch;
  int sblimit = bsi.sblimit;
  int jsbound = bsi.jsbound;
  int table   = II_table;

  int16_t  bit_alloc[MPA_NCH][SBLIMIT]; 
  sample_t scale[MPA_NCH][3][SBLIMIT];
  
  /////////////////////////////////////////////////////////
  // Load bitalloc
 
  const int16_t *ba_bits = II_ba_bits_tbl[table];

  if (nch == 1)
  {
    for (sb = 0; sb < sblimit; sb++)
    {
      int bits = ba_bits[sb];
      bit_alloc[0][sb] = II_ba_tbl[table][sb][bs.get(bits)];
    }
    
    for (sb = sblimit; sb < SBLIMIT; sb++) 
      bit_alloc[0][sb] = 0;
  }
  else
  {
    for (sb = 0; sb < jsbound; sb++) 
    {
      int bits = ba_bits[sb];
      if (bits)
      {
        bit_alloc[0][sb] = II_ba_tbl[table][sb][bs.get(bits)];
        bit_alloc[1][sb] = II_ba_tbl[table][sb][bs.get(bits)];
      }
      else
      {
        bit_alloc[0][sb] = II_ba_tbl[table][sb][0];
        bit_alloc[1][sb] = II_ba_tbl[table][sb][0];
      }
    }

    for (sb = jsbound; sb < sblimit; sb++)
    {
      int bits = ba_bits[sb];
      if (bits)
        bit_alloc[0][sb] = bit_alloc[1][sb] = II_ba_tbl[table][sb][bs.get(bits)];
      else
        bit_alloc[0][sb] = bit_alloc[1][sb] = II_ba_tbl[table][sb][0];
    }
    
    for (sb = sblimit; sb < SBLIMIT; sb++) 
      bit_alloc[0][sb] = bit_alloc[1][sb] = 0;
  }

  /////////////////////////////////////////////////////////
  // Load scalefactors bitalloc
  
  uint16_t scfsi[2][SBLIMIT];
  for (sb = 0; sb < sblimit; sb++) 
    for (ch = 0; ch < nch; ch++)    // 2 bit scfsi 
      if (bit_alloc[ch][sb]) 
        scfsi[ch][sb] = (uint16_t) bs.get(2);

  // do we need this?
  for (sb = sblimit; sb < SBLIMIT; sb++) 
    for (ch = 0; ch < nch; ch++)   
      scfsi[ch][sb] = 0;

  /////////////////////////////////////////////////////////
  // CRC check

  if (!crc_check(frame, bs.get_pos_bits() - 32 - 16))
    return false;

  /////////////////////////////////////////////////////////
  // Load scalefactors

  sample_t c;
  for (sb = 0; sb < sblimit; sb++) 
    for (ch = 0; ch < nch; ch ++) 
    {
      int ba = bit_alloc[ch][sb];
      if (ba)
      {
        if (ba > 0)
          c = c_tbl[ba];
        else 
          switch (ba)
          {
          case -5:  c = c_tbl[0]; break;
          case -7:  c = c_tbl[1]; break;
          case -10: c = c_tbl[2]; break;
          }

        switch (scfsi[ch][sb]) 
        {
          case 0 :  // all three scale factors transmitted 
            scale[ch][0][sb] = scale_tbl[bs.get(6)] * c;
            scale[ch][1][sb] = scale_tbl[bs.get(6)] * c;
            scale[ch][2][sb] = scale_tbl[bs.get(6)] * c;
            break;
          
          case 1 :  // scale factor 1 & 3 transmitted 
            scale[ch][0][sb] =
            scale[ch][1][sb] = scale_tbl[bs.get(6)] * c;
            scale[ch][2][sb] = scale_tbl[bs.get(6)] * c;
            break;
          
          case 3 :  // scale factor 1 & 2 transmitted
            scale[ch][0][sb] = scale_tbl[bs.get(6)] * c;
            scale[ch][1][sb] =
            scale[ch][2][sb] = scale_tbl[bs.get(6)] * c;
            break;
          
          case 2 :    // only one scale factor transmitted
            scale[ch][0][sb] =
            scale[ch][1][sb] =
            scale[ch][2][sb] = scale_tbl[bs.get(6)] * c;
            break;
          
          default : break;      
        }
      }
      else 
      {
        scale[ch][0][sb] = scale[ch][1][sb] =
          scale[ch][2][sb] = 0;         
      }         
    }

  // do we need this?
  for (sb = sblimit; sb < SBLIMIT; sb++) 
    for (ch = 0; ch < nch; ch++) 
      scale[ch][0][sb] = scale[ch][1][sb] =
        scale[ch][2][sb] = 0;

  /////////////////////////////////////////////////////////
  // Decode fraction and synthesis

  sample_t *sptr[MPA_NCH];

  for (int i = 0; i < SCALE_BLOCK; i++) 
  {
    sptr[0] = &samples[0][i * SBLIMIT * 3];
    sptr[1] = &samples[1][i * SBLIMIT * 3];
    II_decode_fraction(sptr, bit_alloc, scale, i >> 2);
    for (ch = 0; ch < nch; ch++)
    {
      synth[ch]->synth(&samples[ch][i * SBLIMIT * 3              ]);
      synth[ch]->synth(&samples[ch][i * SBLIMIT * 3 + 1 * SBLIMIT]);
      synth[ch]->synth(&samples[ch][i * SBLIMIT * 3 + 2 * SBLIMIT]);
    }
  }

  return true;
}


void 
MPAParser::II_decode_fraction(
  sample_t *fraction[MPA_NCH],
  int16_t  bit_alloc[MPA_NCH][SBLIMIT],
  sample_t scale[MPA_NCH][3][SBLIMIT],
  int x)
{
  int sb, ch;
  int nch     = bsi.nch;
  int sblimit = bsi.sblimit;
  int jsbound = bsi.jsbound;

  uint16_t s0, s1, s2;
  int16_t d;
  int16_t ba; // signed!

  for (sb = 0; sb < sblimit; sb++) 
    for (ch = 0; ch < ((sb < jsbound)? nch: 1 ); ch++) 
    {
      // ba means number of bits to read;
      // negative numbers mean sample triplets
      ba = bit_alloc[ch][sb];

      if (ba) 
      {
        if (ba > 0)
        {                                        
          d  = d_tbl[ba]; // ba > 0 => ba = quant
          s0 = (uint16_t) bs.get(ba);
          s1 = (uint16_t) bs.get(ba);
          s2 = (uint16_t) bs.get(ba);

          ba = 16 - ba;  // number of bits we should shift
        }
        else // nlevels = 3, 5, 9; ba = -5, -7, -10
        {  
          // packed triplet of samples
          ba = -ba;
          unsigned int pack = (unsigned int) bs.get(ba);
          switch (ba)
          {
          case 5:
            s0 = pack % 3; pack /= 3;
            s1 = pack % 3; pack /= 3;
            s2 = pack % 3;
            d  = d_tbl[0];
            ba = 14;
            break;

          case 7:
            s0 = pack % 5; pack /= 5;
            s1 = pack % 5; pack /= 5;
            s2 = pack % 5;
            d  = d_tbl[1];
            ba = 13;
            break;

          case 10:
            s0 = pack % 9; pack /= 9;
            s1 = pack % 9; pack /= 9;
            s2 = pack % 9;
            d  = d_tbl[2];
            ba = 12;
            break;

          default:
            assert(false);
            return;
          } 
        } // if (ba > 0) .. else ..

        #define dequantize(r, s, d, bits)                     \
        {                                                     \
          s = ((unsigned short) s) << bits;                   \
          s = (s & 0x7fff) | (~s & 0x8000);                   \
          r = (sample_t)((short)(s) + d) * scale[ch][x][sb];  \
        }

        #define dequantize2(r1, r2, s, d, bits)               \
        {                                                     \
          s  = ((unsigned short) s) << bits;                  \
          s  = (s & 0x7fff) | (~s & 0x8000);                  \
          sample_t f = sample_t((short)(s) + d);              \
          r1 = f * scale[0][x][sb];                           \
          r2 = f * scale[1][x][sb];                           \
        }

        if (nch > 1 && sb >= jsbound)
        {
          dequantize2(fraction[0][sb              ], fraction[1][sb              ], s0, d, ba);
          dequantize2(fraction[0][sb + 1 * SBLIMIT], fraction[1][sb + 1 * SBLIMIT], s1, d, ba);
          dequantize2(fraction[0][sb + 2 * SBLIMIT], fraction[1][sb + 2 * SBLIMIT], s2, d, ba);
        }
        else
        {
          dequantize(fraction[ch][sb              ], s0, d, ba);
          dequantize(fraction[ch][sb + 1 * SBLIMIT], s1, d, ba);
          dequantize(fraction[ch][sb + 2 * SBLIMIT], s2, d, ba);
        }
      }
      else // ba = 0; no sample transmitted 
      {         
        fraction[ch][sb              ] = 0.0;
        fraction[ch][sb + 1 * SBLIMIT] = 0.0;
        fraction[ch][sb + 2 * SBLIMIT] = 0.0;
        if (nch > 1 && sb >= jsbound)
        {
          fraction[1][sb              ] = 0.0;
          fraction[1][sb + 1 * SBLIMIT] = 0.0;
          fraction[1][sb + 2 * SBLIMIT] = 0.0;
        }
      } // if (ba) ... else ...
    } // for (ch = 0; ch < ((sb < jsbound)? nch: 1 ); ch++)
  // for (sb = 0; sb < sblimit; sb++)
    
  for (ch = 0; ch < nch; ch++) 
    for (sb = sblimit; sb < SBLIMIT; sb++) 
    {
      fraction[ch][sb              ] = 0.0;
      fraction[ch][sb +     SBLIMIT] = 0.0;
      fraction[ch][sb + 2 * SBLIMIT] = 0.0;
    }
}


///////////////////////////////////////////////////////////////////////////////
//  Layer I
///////////////////////////////////////////////////////////////////////////////


bool 
MPAParser::I_decode_frame(const uint8_t *frame)
{
  int ch, sb;
  int nch     = bsi.nch;
  int jsbound = bsi.jsbound;
  
  int16_t  bit_alloc[MPA_NCH][SBLIMIT]; 
  sample_t scale[MPA_NCH][SBLIMIT];
  
  /////////////////////////////////////////////////////////
  // Load bitalloc
  
  for (sb = 0; sb < jsbound; sb++) 
    for (ch = 0; ch < nch; ch++)
      bit_alloc[ch][sb] = bs.get(4);
    
  for (sb = jsbound; sb < SBLIMIT; sb++) 
    bit_alloc[0][sb] = bit_alloc[1][sb] = bs.get(4);
    
  /////////////////////////////////////////////////////////
  // CRC check

  if (!crc_check(frame, bs.get_pos_bits() - 32 - 16))
    return false;

  /////////////////////////////////////////////////////////
  // Load scale

  for (sb = 0; sb < SBLIMIT; sb++) 
    for (ch = 0; ch < nch; ch++)
      if (bit_alloc[ch][sb])
        scale[ch][sb] = scale_tbl[bs.get(6)]; // 6 bit per scale factor
      else                    
        scale[ch][sb] = 0;
      
  /////////////////////////////////////////////////////////
  // Decode fraction and synthesis

  sample_t *sptr[2];
  for (int i = 0; i < SCALE_BLOCK * SBLIMIT; i += SBLIMIT) 
  {
    sptr[0] = &samples[0][i];
    sptr[1] = &samples[1][i];
    I_decode_fraction(sptr, bit_alloc, scale);
    for (ch = 0; ch < nch; ch++)
      synth[ch]->synth(&samples[ch][i]);
  }

  return true;
}



void 
MPAParser::I_decode_fraction(
  sample_t *fraction[MPA_NCH],
  int16_t  bit_alloc[MPA_NCH][SBLIMIT],
  sample_t scale[MPA_NCH][SBLIMIT])
{
  int sb, ch;
  int nch     = bsi.nch;
  int jsbound = bsi.jsbound;

  int sample[MPA_NCH][SBLIMIT];
  int ba;

  /////////////////////////////////////////////////////////
  // buffer samples

  for (sb = 0; sb < jsbound; sb++) 
    for (ch = 0; ch < nch; ch++)
    {
      ba = bit_alloc[ch][sb];
      if (ba)
        sample[ch][sb] = (unsigned int) bs.get(ba + 1);
      else 
        sample[ch][sb] = 0;
    }  

  for (sb = jsbound; sb < SBLIMIT; sb++) 
  {
    ba = bit_alloc[0][sb];
    int s;
    if (ba)
      s = (unsigned int) bs.get(ba + 1);
    else 
      s = 0;

    for (ch = 0; ch < nch; ch++)
      sample[ch][sb] = s;
  }      
  
  /////////////////////////////////////////////////////////
  // Dequantize
  
  for (sb = 0; sb < SBLIMIT; sb++)
    for (ch = 0; ch < nch; ch++)
      if (bit_alloc[ch][sb]) 
      {
        ba = bit_alloc[ch][sb] + 1;
        
        if (((sample[ch][sb] >> (ba-1)) & 1) == 1)
          fraction[ch][sb] = 0.0;
        else 
          fraction[ch][sb] = -1.0;

        fraction[ch][sb] += (sample_t) (sample[ch][sb] & ((1<<(ba-1))-1)) /
          (sample_t) (1L<<(ba-1));
        
        fraction[ch][sb] =
          (sample_t) (fraction[ch][sb]+1.0/(sample_t)(1L<<(ba-1))) *
          (sample_t) (1L<<ba) / (sample_t) ((1L<<ba)-1);
      }
      else 
        fraction[ch][sb] = 0.0;
      
  /////////////////////////////////////////////////////////
  // Denormalize
      
  for (ch = 0; ch < nch; ch++)
    for (sb = 0; sb < SBLIMIT; sb++) 
      fraction[ch][sb] *= scale[ch][sb];
}
