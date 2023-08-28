#include <math.h>
#include <stdio.h>
#include "dts_parser.h"
#include "dts_header.h"

#include "dts_tables.h"
#include "dts_tables_huffman.h"
#include "dts_tables_quantization.h"
#include "dts_tables_adpcm.h"
#include "dts_tables_fir.h"
#include "dts_tables_vq.h"

#define DTS_MODE_MONO           0
#define DTS_MODE_CHANNEL        1
#define DTS_MODE_STEREO         2
#define DTS_MODE_STEREO_SUMDIFF 3
#define DTS_MODE_STEREO_TOTAL   4
#define DTS_MODE_3F             5
#define DTS_MODE_2F1R           6
#define DTS_MODE_3F1R           7
#define DTS_MODE_2F2R           8
#define DTS_MODE_3F2R           9

static const int dts_order[NCHANNELS] = { CH_C, CH_L, CH_R, CH_SL, CH_SR, CH_LFE };

static const int amode2mask_tbl[] = 
{
  MODE_MONO, 
  MODE_STEREO,
  MODE_STEREO,
  MODE_STEREO,
  MODE_STEREO,
  MODE_3_0,
  MODE_2_1,
  MODE_3_1,
  MODE_2_2,
  MODE_3_2
};

static const int amode2rel_tbl[] = 
{
  NO_RELATION,
  NO_RELATION,
  NO_RELATION,
  RELATION_SUMDIFF,
  RELATION_DOLBY,
  NO_RELATION,
  NO_RELATION,
  NO_RELATION,
  NO_RELATION,
  NO_RELATION,
};

inline int 
decode_blockcode(int code, int levels, int *values)
{ 
  int i;
  int offset = (levels - 1) >> 1;
  
  for (i = 0; i < 4; i++)
  {
    values[i] = (code % levels) - offset;
    code /= levels;
  }
  
  if (code == 0)
    return 1;
  else
  {
    fprintf (stderr, "ERROR: block code look-up failed\n");
    return 0;
  }
}







///////////////////////////////////////////////////////////////////////////////
// DTSParser
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Init

DTSParser::DTSParser()
{
  init_cosmod();
  samples.allocate(DTS_NCHANNELS, DTS_MAX_SAMPLES);
  reset();
}

void 
DTSParser::init_cosmod()
{
  int i, j, k;
  
  for (j=0,k=0;k<16;k++)
    for (i=0;i<16;i++)
      cos_mod[j++] = cos((2*i+1)*(2*k+1)*M_PI/64);
    
  for (k=0;k<16;k++)
    for (i=0;i<16;i++)
      cos_mod[j++] = cos((i)*(2*k+1)*M_PI/32);
      
  for (k=0;k<16;k++)
    cos_mod[j++] = 0.25/(2*cos((2*k+1)*M_PI/128));
  
  for (k=0;k<16;k++)
    cos_mod[j++] = -0.25/(2.0*sin((2*k+1)*M_PI/128));
}

///////////////////////////////////////////////////////////////////////////////
// FrameParser overrides

const HeaderParser *
DTSParser::header_parser() const
{
  return &dts_header;
}


void 
DTSParser::reset()
{
  spk = spk_unknown;
  frame_size = 0;
  nsamples = 0;

  frame_type      = 0;
  samples_deficit = 0;
  crc_present     = 0;
  sample_blocks   = 0;
  amode           = 0;
  sample_rate     = 0;
  bit_rate        = 0;

  memset(subband_samples_hist, 0, sizeof(subband_samples_hist));
  memset(subband_fir_hist, 0, sizeof(subband_fir_hist));
  memset(subband_fir_noidea, 0, sizeof(subband_fir_noidea));
  memset(lfe_data, 0, sizeof(lfe_data));
}

bool
DTSParser::parse_frame(uint8_t *frame, size_t size)
{
  HeaderInfo hinfo;

  if (!dts_header.parse_header(frame, &hinfo))
    return false;

  if (hinfo.frame_size > size)
    return false;

  spk = hinfo.spk;
  spk.format = FORMAT_LINEAR;
  frame_size = hinfo.frame_size;
  nsamples = hinfo.nsamples;
  bs_type = hinfo.bs_type;

  /////////////////////////////////////////////////////////
  // Convert the bitstream type
  // Note that we have to allocate a buffer for 14bit
  // stream, because the size of the stream increases
  // during conversion.

  if (bs_type == BITSTREAM_14BE || bs_type == BITSTREAM_14LE)
  {
    // Buffered conversion
    if (frame_buf.size() < size * 8 / 7)
      if (frame_buf.allocate(size * 8 / 7))
        return false;

    size_t data_size = bs_convert(frame, size, bs_type, frame_buf, BITSTREAM_8);
    if (data_size == 0)
      return false;

    bs.set(frame_buf, 0, data_size * 8);
  }
  else
  {
    // Inplace conversion
    size_t data_size = bs_convert(frame, size, bs_type, frame, BITSTREAM_8);
    if (data_size == 0)
      return false;

    bs.set(frame, 0, data_size * 8);
  }

  if (!parse_frame_header())
    return false;

  // 8 samples per subsubframe and per subband
  for (int i = 0; i < sample_blocks / 8; i++) 
  {
    // Sanity check
    if (current_subframe >= subframes)
      return false;
  
    // Read subframe header
    if (!current_subsubframe)
      if (!parse_subframe_header())
        return false;
  
    // Read subsubframe
    if (!parse_subsubframe()) 
      return false;
  
    // Update state
    current_subsubframe++;
    if (current_subsubframe >= subsubframes)
    {
      current_subsubframe = 0;
      current_subframe++;
    }

    // Read subframe footer
    if (current_subframe >= subframes)
      if (!parse_subframe_footer()) 
        return false;
  }

  return true;
}

size_t
DTSParser::stream_info(char *buf, size_t size) const
{
  char info[1024];

  char *stream = "???";
  switch (bs_type)
  {
    case BITSTREAM_16BE: stream = "16bit BE"; break;
    case BITSTREAM_16LE: stream = "16bit LE"; break;
    case BITSTREAM_14BE: stream = "14bit BE"; break;
    case BITSTREAM_14LE: stream = "14bit LE"; break;
  }

  size_t len = sprintf(info,
    "DTS\n"
    "speakers:  %s\n"
    "sample rate: %iHz\n"
    "bitrate: %ikbps\n"
    "stream: %s\n"
    "frame size: %i bytes\n"
    "nsamples: %i\n"
    "amode: %i\n"
    "%s"
    "\0",
    spk.mode_text(),
    spk.sample_rate,
    dts_bit_rates[bit_rate] / 1000,
    stream, 
    frame_size,
    nsamples,
    amode,
    crc_present? "CRC protected\n": "No CRC"
    );

  if (len + 1 > size) len = size - 1;
  memcpy(buf, info, len + 1);
  buf[len] = 0;
  return len;
}

size_t
DTSParser::frame_info(char *buf, size_t size) const 
{
  if (buf && size) buf[0] = 0;
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//                         FRAME HEADER
///////////////////////////////////////////////////////////////////////////////



bool 
DTSParser::parse_frame_header()
{
  int i, j;
  static const float adj_table[] = { 1.0, 1.1250, 1.2500, 1.4375 };

  /////////////////////////////////////////////////////////
  // Skip sync

  bs.get(32);

  /////////////////////////////////////////////////////////
  // Frame header

  frame_type        = bs.get(1);
  samples_deficit   = bs.get(5)  + 1;
  crc_present       = bs.get(1);
  sample_blocks     = bs.get(7)  + 1;
  frame_size        = bs.get(14) + 1;
  amode             = bs.get(6);
  sample_rate       = bs.get(4);
  bit_rate          = bs.get(5);

  downmix           = bs.get(1);
  dynrange          = bs.get(1);
  timestamp         = bs.get(1);
  aux_data          = bs.get(1);
  hdcd              = bs.get(1);
  ext_descr         = bs.get(3);
  ext_coding        = bs.get(1);
  aspf              = bs.get(1);
  lfe               = bs.get(2);
  predictor_history = bs.get(1);

  if (crc_present) 
  {
    // todo: check CRC
    header_crc = bs.get(16);
  }

  multirate_inter   = bs.get(1);
  version           = bs.get(4);
  copy_history      = bs.get(2);
  source_pcm_res    = bs.get(3);
  front_sum         = bs.get(1);
  surround_sum      = bs.get(1);
  dialog_norm       = bs.get(4);

  /////////////////////////////////////////////////////////
  // Primary audio coding header

  subframes         = bs.get(4) + 1;
  prim_channels     = bs.get(3) + 1;

  for (i = 0; i < prim_channels; i++)
  {
    subband_activity[i] = bs.get(5) + 2;
    if (subband_activity[i] > DTS_SUBBANDS)
      subband_activity[i] = DTS_SUBBANDS;
  }

  for (i = 0; i < prim_channels; i++)
  {
    vq_start_subband[i] = bs.get(5) + 1;
    if (vq_start_subband[i] > DTS_SUBBANDS)
      vq_start_subband[i] = DTS_SUBBANDS;
  }

  for (i = 0; i < prim_channels; i++)
    joint_intensity[i] = bs.get(3);

  for (i = 0; i < prim_channels; i++)
    transient_huffman[i] = bs.get(2);

  for (i = 0; i < prim_channels; i++)
    scalefactor_huffman[i] = bs.get(3);

  for (i = 0; i < prim_channels; i++)
  {
    bitalloc_huffman[i] = bs.get(3);
    // if (bitalloc_huffman[i] == 7) bailout
  }

  // Get codebooks quantization indexes
  for (i = 0; i < prim_channels; i++)
  {
    quant_index_huffman[i][0] = 0; // Not transmitted
    quant_index_huffman[i][1] = bs.get(1);
  }

  for (j = 2; j < 6; j++)
    for (i = 0; i < prim_channels; i++)
      quant_index_huffman[i][j] = bs.get(2);

  for (j = 6; j < 11; j++)
    for (i = 0; i < prim_channels; i++)
      quant_index_huffman[i][j] = bs.get(3);

  for (j = 11; j < 27; j++)
    for (i = 0; i < prim_channels; i++)
      quant_index_huffman[i][j] = 0; // Not transmitted

  // Get scale factor adjustment
  for (j = 0; j < 11; j++)
    for (i = 0; i < prim_channels; i++)
      scalefactor_adj[i][j] = 1;

  for (i = 0; i < prim_channels; i++)
    if (quant_index_huffman[i][1] == 0)
      scalefactor_adj[i][1] = adj_table[bs.get(2)];

  for (j = 2; j < 6; j++)
    for (i = 0; i < prim_channels; i++)
      if (quant_index_huffman[i][j] < 3)
        scalefactor_adj[i][j] = adj_table[bs.get(2)];

  for (j = 6; j < 11; j++)
    for (i = 0; i < prim_channels; i++)
      if (quant_index_huffman[i][j] < 7)
        scalefactor_adj[i][j] = adj_table[bs.get(2)];

  if (crc_present)
  {
    // todo: audio header CRC check
    bs.get(16);
  }

  /////////////////////////////////////////////////////////
  // Some initializations

  current_subframe = 0;
  current_subsubframe = 0;

  if (amode >= sizeof(amode2mask_tbl) / sizeof(amode2mask_tbl[0]))
    return false;
/*
  if (bs.get_type() == BITSTREAM_14LE ||
      bs.get_type() == BITSTREAM_14BE)
    frame_size = frame_size * 16 / 14;

  int mask = amode2mask_tbl[amode];
  int relation = amode2rel_tbl[amode];
  if (lfe) mask |= CH_MASK_LFE;
  spk.set(FORMAT_DTS, mask, dts_sample_rates[sample_rate], 1.0, relation);

  // todo: support short frames
  nsamples = sample_blocks * 32;
*/
  return true;
}



///////////////////////////////////////////////////////////////////////////////
//                         SUBFRAME HEADER
///////////////////////////////////////////////////////////////////////////////



bool 
DTSParser::parse_subframe_header()
{
  // Primary audio coding side information
  int ch, k;
  
  // Subsubframe count
  subsubframes = bs.get(2) + 1;
  
  // Partial subsubframe sample count
  partial_samples = bs.get(3);
  
  // Get prediction mode for each subband
  for (ch = 0; ch < prim_channels; ch++)
    for (k = 0; k < subband_activity[ch]; k++)
      prediction_mode[ch][k] = bs.get(1);
  
  // Get prediction codebook
  for (ch = 0; ch < prim_channels; ch++)
    for (k = 0; k < subband_activity[ch]; k++)
      if (prediction_mode[ch][k] > 0)
        // (Prediction coefficient VQ address)
        prediction_vq[ch][k] = bs.get(12);
  
  // Bit allocation index
  for (ch = 0; ch < prim_channels; ch++)
  {
    for (k = 0; k < vq_start_subband[ch]; k++)
    {
      if (bitalloc_huffman[ch] == 6)
        bitalloc[ch][k] = bs.get(5);
      else if (bitalloc_huffman[ch] == 5)
        bitalloc[ch][k] = bs.get(4);
      else
        bitalloc[ch][k] = InverseQ(bitalloc_12[bitalloc_huffman[ch]]);
      
      if (bitalloc[ch][k] > 26)
      {
        fprintf (stderr, "bitalloc index [%i][%i] too big (%i)\n",
          ch, k, bitalloc[ch][k]);
        return false;
      }
    }    
  }
  
  // Transition mode
  for (ch = 0; ch < prim_channels; ch++)
  {
    for (k = 0; k < subband_activity[ch]; k++)
    {
      transition_mode[ch][k] = 0;
      if (subsubframes > 1 && k < vq_start_subband[ch] && bitalloc[ch][k] > 0)
        transition_mode[ch][k] = InverseQ(tmode[transient_huffman[ch]]);
    }
  }
  
  // Scale factors
  for (ch = 0; ch < prim_channels; ch++)
  {
    const int *scale_table;
    int scale_sum;
    
    for (k = 0; k < subband_activity[ch]; k++)
    {
      scale_factor[ch][k][0] = 0;
      scale_factor[ch][k][1] = 0;
    }
    
    if (scalefactor_huffman[ch] == 6)
      scale_table = scale_factor_quant7;
    else
      scale_table = scale_factor_quant6;
    
    // When huffman coded, only the difference is encoded
    scale_sum = 0;
    
    for (k = 0; k < subband_activity[ch]; k++)
    {
      if (k >= vq_start_subband[ch] || bitalloc[ch][k] > 0)
      {
        if (scalefactor_huffman[ch] < 5)
          // huffman encoded
          scale_sum += InverseQ(scales_129[scalefactor_huffman[ch]]);
        else if (scalefactor_huffman[ch] == 5)
          scale_sum = bs.get(6);
        else if (scalefactor_huffman[ch] == 6)
          scale_sum = bs.get(7);
        
        scale_factor[ch][k][0] = scale_table[scale_sum];
      }
      
      if (k < vq_start_subband[ch] && transition_mode[ch][k])
      {
        // Get second scale factor
        if (scalefactor_huffman[ch] < 5)
          // huffman encoded
          scale_sum += InverseQ(scales_129[scalefactor_huffman[ch]]);
        else if (scalefactor_huffman[ch] == 5)
          scale_sum = bs.get(6);
        else if (scalefactor_huffman[ch] == 6)
          scale_sum = bs.get(7);
        
        scale_factor[ch][k][1] = scale_table[scale_sum];
      }
    }
    
  }
  
  // Joint subband scale factor codebook select
  for (ch = 0; ch < prim_channels; ch++)
    if (joint_intensity[ch] > 0)
      joint_huff[ch] = bs.get(3);
  
  // Scale factors for joint subband coding
  for (ch = 0; ch < prim_channels; ch++)
  {
    int source_channel;
    
    // Transmitted only if joint subband coding enabled
    if (joint_intensity[ch] > 0)
    {
      int scale = 0;
      source_channel = joint_intensity[ch] - 1;
      
      // When huffman coded, only the difference is encoded
      // (is this valid as well for joint scales ???)
      
      for (k = subband_activity[ch];
      k < subband_activity[source_channel]; k++)
      {
        if (joint_huff[ch] < 5)
          // huffman encoded
          scale = InverseQ(scales_129[joint_huff[ch]]);
        else if (joint_huff[ch] == 5)
          scale = bs.get(6);
        else if (joint_huff[ch] == 6)
          scale = bs.get(7);
        
        scale += 64; // bias
        joint_scale_factor[ch][k] = scale;//joint_scale_table[scale];*/
      }
      
      //if (!debug_flag & 0x02)
      //{
      //  fprintf (stderr, "Joint stereo coding not supported\n");
      //  debug_flag |= 0x02;
      //}
      
    }
  }
  
  // Stereo downmix coefficients
  if (prim_channels > 2 && downmix)
  {
    for (ch = 0; ch < prim_channels; ch++)
    {
      // ???????????????????????????
      downmix_coef[ch][0] = bs.get(7);
      downmix_coef[ch][1] = bs.get(7);
    }
  }
  
  // Dynamic range coefficient
  if (dynrange) dynrange_coef = bs.get(8);
  
  // Side information CRC check word
  if (crc_present)
    bs.get(16);
  
  /////////////////////////////////////////////////////////
  // Primary audio data arrays
  
  // VQ encoded high frequency subbands
  for (ch = 0; ch < prim_channels; ch++)
    for (k = vq_start_subband[ch]; k < subband_activity[ch]; k++)
      // 1 vector -> 32 samples
      high_freq_vq[ch][k] = bs.get(10);      
  
  // Low frequency effect data
  if (lfe)
  {
    // LFE samples
    int lfe_samples = 2 * lfe * subsubframes;
    double lfe_scale;
    
    for (k = lfe_samples; k < lfe_samples * 2; k++)
      // Signed 8 bits int
      lfe_data[k] = bs.get_signed(8);
    
    // Scale factor index
    lfe_scale_factor = scale_factor_quant7[bs.get(8)];
    
    // Quantization step size * scale factor
    lfe_scale = 0.035 * lfe_scale_factor;
    
    for (k = lfe_samples; k < lfe_samples * 2; k++)
      lfe_data[k] *= lfe_scale;
    
  }
  
  return true;
}



///////////////////////////////////////////////////////////////////////////////
//                         SUBSUBFRAME
///////////////////////////////////////////////////////////////////////////////


bool
DTSParser::parse_subsubframe()
{
  int ch, l;
  int subsubframe = current_subsubframe;
  
  const double *quant_step_table;
  
  // FIXME
  double subband_samples[DTS_PRIM_CHANNELS_MAX][DTS_SUBBANDS][8];
  
  /////////////////////////////////////////////////////////
  // Audio data
  
  // Select quantization step size table
  if (bit_rate == 0x1f) 
    quant_step_table = lossless_quant_d;
  else
    quant_step_table = lossy_quant_d;
  
  for (ch = 0; ch < prim_channels; ch++)
  {
    for (l = 0; l < vq_start_subband[ch] ; l++)
    {
      int m;
      
      // Select the mid-tread linear quantizer
      int abits = bitalloc[ch][l];
      
      double quant_step_size = quant_step_table[abits];
      double rscale;
      
      /////////////////////////////////////////////////////
      // Determine quantization index code book and its type 
      
      // Select quantization index code book
      int sel = quant_index_huffman[ch][abits]; 
      
      // Determine its type
      int q_type = 1; // (Assume Huffman type by default)
      if (abits >= 11 || !bitalloc_select[abits][sel])
      {
        // Not Huffman type
        if (abits <= 7) 
          q_type = 3; // Block code
        else 
          q_type = 2; // No further encoding
      }
      
      if (abits == 0) q_type = 0; // No bits allocated
      
      /////////////////////////////////////////////////////
      // Extract bits from the bit stream 

      switch (q_type)
      {
        case 0: // No bits allocated
          for (m=0; m<8; m++)
            subband_samples[ch][l][m] = 0;
          break;
        
        case 1: // Huffman code
          for (m=0; m<8; m++)
            subband_samples[ch][l][m] = InverseQ(bitalloc_select[abits][sel]);
          break;
        
        case 2: // No further encoding
          for (m=0; m<8; m++)
          {
            // Extract (signed) quantization index
            int q_index = bs.get(abits - 3);
            if (q_index & (1 << (abits - 4)))
            {
              q_index = (1 << (abits - 3)) - q_index;
              q_index = -q_index;
            }
            subband_samples[ch][l][m] = q_index;
          }
          break;
        
        case 3: // Block code
          {
            int block_code1, block_code2, size, levels;
            int block[8];
          
            switch (abits)
            {
            case 1:
              size = 7;
              levels = 3;
              break;
            case 2:
              size = 10;
              levels = 5;
              break;
            case 3:
              size = 12;
              levels = 7;
              break;
            case 4:
              size = 13;
              levels = 9;
              break;
            case 5:
              size = 15;
              levels = 13;
              break;
            case 6:
              size = 17;
              levels = 17;
              break;
            case 7:
            default:
              size = 19;
              levels = 25;
              break;
            }
          
            block_code1 = bs.get(size);
            // Should test return value
            decode_blockcode (block_code1, levels, block);
            block_code2 = bs.get(size);
            decode_blockcode (block_code2, levels, &block[4]);
            for (m=0; m<8; m++)
              subband_samples[ch][l][m] = block[m];
          
          }
          break;
        
        default: // Undefined
          fprintf (stderr, "Unknown quantization index codebook");
          return false;
      } // switch (q_type)
      
      /////////////////////////////////////////////////////
      // Account for quantization step and scale factor
      
      // Deal with transients
      if (transition_mode[ch][l] && subsubframe >= transition_mode[ch][l])
        rscale = quant_step_size * scale_factor[ch][l][1];
      else
        rscale = quant_step_size * scale_factor[ch][l][0];
      
      // Adjustment
      rscale *= scalefactor_adj[ch][sel];
      for (m = 0; m < 8; m++) 
        subband_samples[ch][l][m] *= rscale;
      
      /////////////////////////////////////////////////////
      // Inverse ADPCM if in prediction mode

      if (prediction_mode[ch][l])
      {
        int n;
        for (m = 0; m < 8; m++)
        {
          for (n = 1; n <= 4; n++)
          {
            if (m-n >= 0)
              subband_samples[ch][l][m] +=
                (adpcm_vb[prediction_vq[ch][l]][n-1] *
                 subband_samples[ch][l][m-n]/8192);
            else if (predictor_history)
              subband_samples[ch][l][m] +=
                (adpcm_vb[prediction_vq[ch][l]][n-1] *
                subband_samples_hist[ch][l][m-n+4]/8192);
          }
        }
      }
    } // for (l = 0; l < vq_start_subband[ch] ; l++)
        
    ///////////////////////////////////////////////////////
    // Decode VQ encoded high frequencies

    for (l = vq_start_subband[ch]; l < subband_activity[ch]; l++)
    {
      // 1 vector -> 32 samples but we only need the 8 samples
      // for this subsubframe.
      int m;
      
      //if (!debug_flag & 0x01)
      //{
      //  fprintf (stderr, "Stream with high frequencies VQ coding\n");
      //  debug_flag |= 0x01;
      //}
      
      for (m = 0; m < 8; m++)
      {
        subband_samples[ch][l][m] = 
          high_freq_vq_tbl[high_freq_vq[ch][l]][subsubframe*8+m]
          * (double)scale_factor[ch][l][0] / 16.0;
      }
    } // for (l = vq_start_subband[ch]; l < subband_activity[ch]; l++)
  } // for (ch = 0; ch < prim_channels; ch++)
    
  // Check for DSYNC after subsubframe
  if (aspf || subsubframe == subsubframes - 1)
  {
    if (0xFFFF == bs.get(16)) // 0xFFFF
    {
#ifdef DEBUG
      fprintf( stderr, "Got subframe DSYNC\n" );
#endif
    }
    else
    {
      fprintf( stderr, "Didn't get subframe DSYNC\n" );
    }
  }
  
  // Backup predictor history for adpcm
  for (ch = 0; ch < prim_channels; ch++)
  {
    for (l = 0; l < vq_start_subband[ch] ; l++)
    {
      int m;
      for (m = 0; m < 4; m++)
        subband_samples_hist[ch][l][m] =
        subband_samples[ch][l][4+m];
    }
  }
  
  /////////////////////////////////////////////////////////
  //           SYNTHESIS AND OUTPUT
  /////////////////////////////////////////////////////////

  static const int reorder[10][NCHANNELS] =
  {                    // nch | arrangement
    { 0 },             //  1  | A
    { 0, 1 },          //  2  | A + B (dual mono)
    { 0, 1 },          //  2  | L + R (stereo)
    { 0, 1 },          //  2  | (L+R) + (L-R) (sum-difference)
    { 0, 1 },          //  2  | LT + RT (left & right total)
    { 1, 0, 2 },       //  3  | C + L + R
    { 0, 1, 2 },       //  3  | L + R + S
    { 1, 0, 2, 3 },    //  4  | C + L + R + S
    { 0, 1, 2, 3 },    //  4  | L + R + SL + SR
    { 1, 0, 2, 3, 4 }  //  5  | C + L + R + SL + SR
  };

//    static double pcmr2level_tbl[8] =
//    {32768.0, 32768.0, 524288.0, 524288.0, 1.0, 8388608.0, 8388608.0, 1.0 };

  int base = (current_subframe * subsubframes + current_subsubframe) * 32 * 8;

  // 32 subbands QMF
  for (ch = 0; ch < prim_channels; ch++)
    qmf_32_subbands(ch,
      subband_samples[ch],
      samples[reorder[amode][ch]] + base,
//      pcmr2level_tbl[source_pcm_res]);
      32768.0);

  // Generate LFE samples for this subsubframe FIXME!!!
  if (lfe)
  {
    int lfe_samples = 2 * lfe * subsubframes;
   
    lfe_interpolation_fir (lfe, 2 * lfe,
      lfe_data + lfe_samples + 2 * lfe * subsubframe,
      samples[prim_channels] + base,
      8388608.0);
    // Outputs 20bits pcm samples
  }

  return true;
}



///////////////////////////////////////////////////////////////////////////////
//                         SUBFRAME FOOTER
///////////////////////////////////////////////////////////////////////////////

bool
DTSParser::parse_subframe_footer()
{
  int aux_data_count = 0, i;
  int lfe_samples;

  /////////////////////////////////////////////////////////
  // Unpack optional information

  // Time code stamp
  if (timestamp) 
    bs.get(32);

  // Auxiliary data byte count
  if (aux_data) 
    aux_data_count = bs.get(6);

  // Auxiliary data bytes
  for(i = 0; i < aux_data_count; i++)
    bs.get(8);

  // Optional CRC check bytes
  if (crc_present && (downmix || dynrange))
    bs.get(16);

  // Backup LFE samples history
  lfe_samples = 2 * lfe * subsubframes;
  for (i = 0; i < lfe_samples; i++)
    lfe_data[i] = lfe_data[i+lfe_samples];

  return true;
}


int 
DTSParser::InverseQ(const huff_entry_t *huff)
{
  int value = 0;
  int length = 0, j;
  
  while (1)
  {
    length++;
    value <<= 1;
    value |= bs.get(1);
    
    for (j = 0; huff[j].length != 0 && huff[j].length < length; j++ );
    
    if (huff[j].length == 0) 
      break;
    
    for (; huff[j].length == length; j++)
      if (huff[j].code == value)
        return huff[j].value;
  }
  
  return 0;
}


void 
DTSParser::qmf_32_subbands (int ch, double samples_in[32][8], sample_t *samples_out,
                            double scale)
{           
  const double *prCoeff;
  int i, j, k;
  double raXin[32];

  double *subband_fir  = subband_fir_hist[ch];
  double *subband_fir2 = subband_fir_noidea[ch];

  int nChIndex = 0, NumSubband = 32, nStart = 0, nEnd = 8, nSubIndex;

  // Select filter
  if (!multirate_inter) 
    // Non-perfect reconstruction
    prCoeff = fir_32bands_nonperfect;
  else 
    // Perfect reconstruction
    prCoeff = fir_32bands_perfect;

  // Reconstructed channel sample index
  scale = 1.0 / scale;
  for (nSubIndex = nStart; nSubIndex < nEnd; nSubIndex++)
  {
    // Load in one sample from each subband
    for (i = 0; i < subband_activity[ch]; i++)
      raXin[i] = samples_in[i][nSubIndex];

    // Clear inactive subbands
    for (i = subband_activity[ch]; i < NumSubband; i++)
      raXin[i] = 0.0;

    sample_t a, b;
    for (j = 0, k = 0; k < 16; k++, j += 16)
    {
      a  = (raXin[0]  + raXin[1])  * cos_mod[j+0];
      a += (raXin[2]  + raXin[3])  * cos_mod[j+1];
      a += (raXin[4]  + raXin[5])  * cos_mod[j+2];
      a += (raXin[6]  + raXin[7])  * cos_mod[j+3];
      a += (raXin[8]  + raXin[9])  * cos_mod[j+4];
      a += (raXin[10] + raXin[11]) * cos_mod[j+5];
      a += (raXin[12] + raXin[13]) * cos_mod[j+6];
      a += (raXin[14] + raXin[15]) * cos_mod[j+7];
      a += (raXin[16] + raXin[17]) * cos_mod[j+8];
      a += (raXin[18] + raXin[19]) * cos_mod[j+9];
      a += (raXin[20] + raXin[21]) * cos_mod[j+10];
      a += (raXin[22] + raXin[23]) * cos_mod[j+11];
      a += (raXin[24] + raXin[25]) * cos_mod[j+12];
      a += (raXin[26] + raXin[27]) * cos_mod[j+13];
      a += (raXin[28] + raXin[29]) * cos_mod[j+14];
      a += (raXin[30] + raXin[31]) * cos_mod[j+15];

      b  = (raXin[0])              * cos_mod[256 + j+0];
      b += (raXin[2]  + raXin[1])  * cos_mod[256 + j+1];
      b += (raXin[4]  + raXin[3])  * cos_mod[256 + j+2];
      b += (raXin[6]  + raXin[5])  * cos_mod[256 + j+3];
      b += (raXin[8]  + raXin[7])  * cos_mod[256 + j+4];
      b += (raXin[10] + raXin[9])  * cos_mod[256 + j+5];
      b += (raXin[12] + raXin[11]) * cos_mod[256 + j+6];
      b += (raXin[14] + raXin[13]) * cos_mod[256 + j+7];
      b += (raXin[16] + raXin[15]) * cos_mod[256 + j+8];
      b += (raXin[18] + raXin[17]) * cos_mod[256 + j+9];
      b += (raXin[20] + raXin[19]) * cos_mod[256 + j+10];
      b += (raXin[22] + raXin[21]) * cos_mod[256 + j+11];
      b += (raXin[24] + raXin[23]) * cos_mod[256 + j+12];
      b += (raXin[26] + raXin[25]) * cos_mod[256 + j+13];
      b += (raXin[28] + raXin[27]) * cos_mod[256 + j+14];
      b += (raXin[30] + raXin[29]) * cos_mod[256 + j+15];

      subband_fir[k]      = cos_mod[k + 512] * (a+b);
      subband_fir[32-k-1] = cos_mod[k + 528] * (a-b);
    }

    // Multiply by filter coefficients
    for (k = 31, i = 0; i < 32; i++, k--)
    {
      a = subband_fir2[i];
      b = subband_fir2[32+i];

      a += prCoeff[i] * (subband_fir[i] - subband_fir[k]);
      b += prCoeff[32+i]*(-subband_fir[i] - subband_fir[k]);
      a += prCoeff[i+64] * (subband_fir[i+64] - subband_fir[k+64]);
      b += prCoeff[32+i+64]*(-subband_fir[i+64] - subband_fir[k+64]);
      a += prCoeff[i+128] * (subband_fir[i+128] - subband_fir[k+128]);
      b += prCoeff[32+i+128]*(-subband_fir[i+128] - subband_fir[k+128]);
      a += prCoeff[i+192] * (subband_fir[i+192] - subband_fir[k+192]);
      b += prCoeff[32+i+192]*(-subband_fir[i+192] - subband_fir[k+192]);
      a += prCoeff[i+256] * (subband_fir[i+256] - subband_fir[k+256]);
      b += prCoeff[32+i+256]*(-subband_fir[i+256] - subband_fir[k+256]);
      a += prCoeff[i+320] * (subband_fir[i+320] - subband_fir[k+320]);
      b += prCoeff[32+i+320]*(-subband_fir[i+320] - subband_fir[k+320]);
      a += prCoeff[i+384] * (subband_fir[i+384] - subband_fir[k+384]);
      b += prCoeff[32+i+384]*(-subband_fir[i+384] - subband_fir[k+384]);
      a += prCoeff[i+448] * (subband_fir[i+448] - subband_fir[k+448]);
      b += prCoeff[32+i+448]*(-subband_fir[i+448] - subband_fir[k+448]);

      subband_fir2[i] = a;
      subband_fir2[32+i] = b;
    }

    // Create 32 PCM output samples
    for (i = 0; i < 32; i++)
      samples_out[nChIndex++] = subband_fir2[i] * scale;

    // Update working arrays
    for (i = 512-16; i >= 32; i -= 16)
    {
      subband_fir[i+0]  = subband_fir[i-32+0];
      subband_fir[i+1]  = subband_fir[i-32+1];
      subband_fir[i+2]  = subband_fir[i-32+2];
      subband_fir[i+3]  = subband_fir[i-32+3];
      subband_fir[i+4]  = subband_fir[i-32+4];
      subband_fir[i+5]  = subband_fir[i-32+5];
      subband_fir[i+6]  = subband_fir[i-32+6];
      subband_fir[i+7]  = subband_fir[i-32+7];
      subband_fir[i+8]  = subband_fir[i-32+8];
      subband_fir[i+9]  = subband_fir[i-32+9];
      subband_fir[i+10] = subband_fir[i-32+10];
      subband_fir[i+11] = subband_fir[i-32+11];
      subband_fir[i+12] = subband_fir[i-32+12];
      subband_fir[i+13] = subband_fir[i-32+13];
      subband_fir[i+14] = subband_fir[i-32+14];
      subband_fir[i+15] = subband_fir[i-32+15];
    }

    for (i = 0; i < NumSubband; i++)
      subband_fir2[i] = subband_fir2[i+32];

    for (i = 0; i < NumSubband; i++)
      subband_fir2[i+32] = 0.0;

  } // for (nSubIndex=nStart; nSubIndex<nEnd; nSubIndex++)
}

void 
DTSParser::lfe_interpolation_fir(int nDecimationSelect, int nNumDeciSample,
                                 double *samples_in, sample_t *samples_out,
                                 double scale)
{
  // samples_in: An array holding decimated samples.
  //   Samples in current subframe starts from samples_in[0],
  //   while samples_in[-1], samples_in[-2], ..., stores samples
  //   from last subframe as history.
  //
  // samples_out: An array holding interpolated samples
  //

  int nDeciFactor, k, J;
  const double *prCoeff;

  int NumFIRCoef = 512; // Number of FIR coefficients
  int nInterpIndex = 0; // Index to the interpolated samples
  int nDeciIndex;

  // Select decimation filter
  if (nDecimationSelect == 1)
  {
    // 128 decimation
    nDeciFactor = 128;      
    prCoeff = lfe_fir_128;
  } 
  else 
  {
    // 64 decimation
    nDeciFactor = 64;
    prCoeff = lfe_fir_64;
  }

  // Interpolation
  for (nDeciIndex = 0; nDeciIndex < nNumDeciSample; nDeciIndex++)
  {
    // One decimated sample generates nDeciFactor interpolated ones
    for (k = 0; k < nDeciFactor; k++)
    {
      double rTmp = 0.0;
      for (J = 0; J < NumFIRCoef/nDeciFactor; J++)
        rTmp += samples_in[nDeciIndex-J] * prCoeff[k+J*nDeciFactor];
      // Save interpolated samples
      samples_out[nInterpIndex++] = rTmp / scale;
    }
  }
}






