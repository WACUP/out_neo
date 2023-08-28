#include "ac3_header.h"

const AC3Header ac3_header;

static const int halfrate_tbl[12] = 
{ 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3
};
static const int lfe_mask[] = 
{ 
  16, 16, 4, 4, 4, 1, 4, 1
};
static const int bitrate_tbl[] = 
{ 
  32,  40,  48,  56,  64,  80,  96, 112,
 128, 160, 192, 224, 256, 320, 384, 448,
 512, 576, 640 
};
static const int acmod2mask_tbl[] = 
{
  MODE_2_0,
  MODE_1_0, 
  MODE_2_0,
  MODE_3_0,
  MODE_2_1,
  MODE_3_1,
  MODE_2_2,
  MODE_3_2,
  MODE_2_0 | CH_MASK_LFE,
  MODE_1_0 | CH_MASK_LFE, 
  MODE_2_0 | CH_MASK_LFE,
  MODE_3_0 | CH_MASK_LFE,
  MODE_2_1 | CH_MASK_LFE,
  MODE_3_1 | CH_MASK_LFE,
  MODE_2_2 | CH_MASK_LFE,
  MODE_3_2 | CH_MASK_LFE,
};

bool
AC3Header::parse_header(const uint8_t *hdr, HeaderInfo *hinfo) const
{
  int fscod;
  int frmsizecod;

  int acmod;
  int dolby = NO_RELATION;

  int halfrate;
  int bitrate;
  int sample_rate;

  /////////////////////////////////////////////////////////
  // 8 bit or 16 bit big endian stream sync
  if ((hdr[0] == 0x0b) && (hdr[1] == 0x77))
  {
    // constraints
    if (hdr[5] >= 0x60)         return false;   // 'bsid'
    if ((hdr[4] & 0x3f) > 0x25) return false;   // 'frmesizecod'
    if ((hdr[4] & 0xc0) > 0x80) return false;   // 'fscod'
    if (!hinfo) return true;

    fscod      = hdr[4] >> 6;
    frmsizecod = hdr[4] & 0x3f;
    acmod      = hdr[6] >> 5;

    if (acmod == 2 && (hdr[6] & 0x18) == 0x10)
      dolby = RELATION_DOLBY;

    if (hdr[6] & lfe_mask[acmod])
      acmod |= 8;

    halfrate   = halfrate_tbl[hdr[5] >> 3];
    bitrate    = bitrate_tbl[frmsizecod >> 1];

    hinfo->bs_type = BITSTREAM_8;
  }
  /////////////////////////////////////////////////////////
  // 16 bit low endian stream sync
  else if ((hdr[1] == 0x0b) && (hdr[0] == 0x77))
  {
    // constraints
    if (hdr[4] >= 0x60)         return false;   // 'bsid'
    if ((hdr[5] & 0x3f) > 0x25) return false;   // 'frmesizecod'
    if ((hdr[5] & 0xc0) > 0x80) return false;   // 'fscod'
    if (!hinfo) return true;

    fscod      = hdr[5] >> 6;
    frmsizecod = hdr[5] & 0x3f;
    acmod      = hdr[7] >> 5;

    if (acmod == 2 && (hdr[7] & 0x18) == 0x10)
      dolby = RELATION_DOLBY;

    if (hdr[7] & lfe_mask[acmod])
      acmod |= 8;

    halfrate   = halfrate_tbl[hdr[4] >> 3];
    bitrate    = bitrate_tbl[frmsizecod >> 1];

    hinfo->bs_type = BITSTREAM_16LE;
  }
  else
    return false;

  switch (fscod) 
  {
    case 0:    
      hinfo->frame_size = 4 * bitrate;
      sample_rate = 48000 >> halfrate;
      break;

    case 1: 
      hinfo->frame_size = 2 * (320 * bitrate / 147 + (frmsizecod & 1));
      sample_rate = 44100 >> halfrate;
      break;

    case 2: 
      hinfo->frame_size = 6 * bitrate;
      sample_rate = 32000 >> halfrate;

    default:
      return false;
  }

  hinfo->spk = Speakers(FORMAT_AC3, acmod2mask_tbl[acmod], sample_rate, 1.0, dolby);
  hinfo->scan_size = 0; // do not scan
  hinfo->nsamples = 1536;
  hinfo->spdif_type = 1; // SPDIF Pc burst-info (data type = AC3) 
  return true;
}

bool
AC3Header::compare_headers(const uint8_t *hdr1, const uint8_t *hdr2) const
{
  // Compare headers; we must exclude:
  // * crc (bytes 2 and 3)
  // * 'compre' and 'compre' fields
  // * last bit of frmsizcod (used to match the bitrate in 44100Hz mode).
  //
  // Positions of 'compre' and 'compr' fields are determined by 'acmod'
  // field. To exclude them we use masking (acmod2mask table).
  
  static const int acmod2mask[] = { 0x80, 0x80, 0xe0, 0xe0, 0xe0, 0xf8, 0xe0, 0xf8 };

  /////////////////////////////////////////////////////////
  // 8 bit or 16 bit big endian
  if ((hdr1[0] == 0x0b) && (hdr1[1] == 0x77))
  {
    // constraints
    if (hdr1[5] >= 0x60)         return false;   // 'bsid'
    if ((hdr1[4] & 0x3f) > 0x25) return false;   // 'frmesizecod'
    if ((hdr1[4] & 0xc0) > 0x80) return false;   // 'fscod'

    int mask = acmod2mask[hdr1[6] >> 5];
    return
      hdr1[0] == hdr2[0] && hdr1[1] == hdr2[1] &&
      (hdr1[4] & 0xfe) == (hdr2[4] & 0xfe) && hdr1[5] == hdr2[5] &&
      hdr1[6] == hdr2[6] && (hdr1[7] & mask) == (hdr2[7] & mask);
  }
  /////////////////////////////////////////////////////////
  // 16 bit low endian
  else if ((hdr1[1] == 0x0b) && (hdr1[0] == 0x77))
  {
    // constraints
    if (hdr1[4] >= 0x60)         return false;   // 'bsid'
    if ((hdr1[5] & 0x3f) > 0x25) return false;   // 'frmesizecod'
    if ((hdr1[5] & 0xc0) > 0x80) return false;   // 'fscod'

    int mask = acmod2mask[hdr1[7] >> 5];
    return
      hdr1[1] == hdr2[1] && hdr1[0] == hdr2[0] &&
      (hdr1[5] & 0xfe) == (hdr2[5] & 0xfe) && hdr1[4] == hdr2[4] &&
      hdr1[7] == hdr2[7] && (hdr1[6] & mask) == (hdr2[6] & mask);
  }

  return false;
}
