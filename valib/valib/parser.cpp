#include <stdio.h>
#include "parser.h"

///////////////////////////////////////////////////////////////////////////////
// HeaderParser
///////////////////////////////////////////////////////////////////////////////

size_t
HeaderParser::header_info(const uint8_t *hdr, char *buf, size_t size) const
{
  char info[1024];
  HeaderInfo h;
  size_t info_size = 0;

  if (parse_header(hdr, &h))
  {
    info_size += sprintf(info + info_size, "Stream format: %s %s %iHz\n", h.spk.format_text(), h.spk.mode_text(), h.spk.sample_rate);

    switch (h.bs_type)
    {
      case BITSTREAM_8:    info_size += sprintf(info + info_size, "Bitstream type: byte stream\n"); break;
      case BITSTREAM_16BE: info_size += sprintf(info + info_size, "Bitstream type: 16bit big endian\n"); break;
      case BITSTREAM_16LE: info_size += sprintf(info + info_size, "Bitstream type: 16bit low endian\n"); break;
      case BITSTREAM_14BE: info_size += sprintf(info + info_size, "Bitstream type: 14bit big endian\n"); break;
      case BITSTREAM_14LE: info_size += sprintf(info + info_size, "Bitstream type: 14bit low endian\n"); break;
      default:        info_size += sprintf(info + info_size, "Bitstream type: unknown\n"); break;
    }

    if (h.frame_size)
      info_size += sprintf(info + info_size, "Frame size: %i\n", h.frame_size);
    else
      info_size += sprintf(info + info_size, "Frame size: free format\n");

    info_size += sprintf(info + info_size, "Samples: %i\n", h.nsamples);

    if (h.frame_size > 0 && h.nsamples > 0)
      info_size += sprintf(info + info_size, "Bitrate: %ikbps\n", h.frame_size * h.spk.sample_rate * 8 / h.nsamples / 1000);
    else
      info_size += sprintf(info + info_size, "Bitrate: unknown\n");

    if (h.spdif_type)
      info_size += sprintf(info + info_size, "SPDIF stream type: 0x%x\n", h.spdif_type);
  }
  else
  {
    info_size += sprintf(info + info_size, "No header found\n");
  }

  if (info_size > size) info_size = size;
  memcpy(buf, info, info_size);
  buf[info_size] = 0;
  return info_size;
};

///////////////////////////////////////////////////////////////////////////////
// StreamBuffer
///////////////////////////////////////////////////////////////////////////////

StreamBuffer::StreamBuffer()
{
  parser = 0;
  header_size = 0;
  min_frame_size = 0;
  max_frame_size = 0;

  header_buf = 0;
  hinfo.drop();

  sync_buf = 0;
  sync_size = 0;
  sync_data = 0;

  debris = 0;
  debris_size = 0;

  frame = 0;
  frame_size = 0;
  frame_interval = 0;

  in_sync = false;
  new_stream = false;

  frames = 0;
}

StreamBuffer::StreamBuffer(const HeaderParser *_parser)
{
  parser = 0;
  header_size = 0;
  min_frame_size = 0;
  max_frame_size = 0;

  header_buf = 0;
  hinfo.drop();

  sync_buf = 0;
  sync_size = 0;
  sync_data = 0;

  debris = 0;
  debris_size = 0;

  frame = 0;
  frame_size = 0;
  frame_interval = 0;

  in_sync = false;
  new_stream = false;

  frames = 0;

  set_parser(_parser);
}

StreamBuffer::~StreamBuffer()
{}

bool 
StreamBuffer::set_parser(const HeaderParser *_parser)
{
  release_parser();

  if (!_parser) 
    return false;

  if  (!buf.allocate(_parser->max_frame_size() * 3 + _parser->header_size() * 2))
    return false;

  parser         = _parser;
  header_size    = parser->header_size();
  min_frame_size = parser->min_frame_size();
  max_frame_size = parser->max_frame_size();

  header_buf = buf.data();

  sync_buf = buf.data() + header_size;
  sync_size = max_frame_size * 3 + header_size;

  return true;
}

void 
StreamBuffer::release_parser()
{
  reset();

  parser = 0;
  header_size = 0;
  min_frame_size = 0;
  max_frame_size = 0;

  header_buf = 0;
  sync_buf = 0;
  sync_size = 0;
}


void 
StreamBuffer::reset()
{
  hinfo.drop();
  sync_data = 0;
  pre_frame = max_frame_size;

  debris = 0;
  debris_size = 0;

  frame = 0;
  frame_size = 0;
  frame_interval = 0;

  in_sync = false;
  new_stream = false;
}

bool
StreamBuffer::load_buffer(uint8_t **data, uint8_t *end, size_t required_size)
{
  if (required_size <= sync_data)
    return true;

  if (sync_data + (end - *data) < required_size)
  {
    size_t load_size = end - *data;
    memcpy(sync_buf + sync_data, *data, load_size);
    sync_data += load_size;
    *data += load_size;
    return false;
  }

  size_t load_size = required_size - sync_data;
  memcpy(sync_buf + sync_data, *data, load_size);
  sync_data += load_size;
  *data += load_size;
  return true;
}

void
StreamBuffer::drop_buffer(size_t size)
{
  assert(sync_data >= size);
  sync_data -= size;
  memmove(sync_buf, sync_buf + size, sync_data);
}

#define LOAD(required_size) if (!load_buffer(data, end, required_size)) return false;
#define DROP(size) drop_buffer(size);

///////////////////////////////////////////////////////////////////////////////
// When we sync on a new stream we may start syncing in between of two
// syncpoints. The data up to the first syncpoint may be wrongly interpreted
// as PCM data and poping sound may occur before the first frame. To prevent
// this we must consider this part as debris before the first syncpoint.
//
// But amount of this data may not be larger than the maximum frame size:
//
// Case 1: The data before the first frame is a part of the stream
// |--------------+------------------------+---
// | < frame_size |          frame1        |  ....
// |--------------+------------------------+---
//
// Case 2: The data before the first frame is not a part of the stream
// |----------------------------+------------------------+---
// |       > frame_size         |          frame1        |  ....
// |----------------------------+------------------------+---
//
// Therefore if we have not found the syncpoint at first frame_size bytes,
// all this data does not belong to a stream.

bool
StreamBuffer::sync(uint8_t **data, uint8_t *end)
{
  assert(*data <= end);
  assert(!in_sync && !new_stream);
  assert(frame == 0 && frame_size == 0 && frame_interval == 0);

  /////////////////////////////////////////////////////////////////////////////
  // Drop debris

  DROP(debris_size);
  debris = 0;
  debris_size = 0;

  /////////////////////////////////////////////////////////////////////////////
  // Cache data

  if (sync_data < sync_size)
  {
    size_t load_size = MIN(size_t(end - *data), (sync_size - sync_data));
    memcpy(sync_buf + sync_data, *data, load_size);
    sync_data += load_size;
    *data += load_size;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Search 1st syncpoint

  HeaderInfo hinfo1, hinfo2;
  uint8_t *frame1, *frame2, *frame3;
  uint8_t *frame1_max, *frame2_max, *frame3_max;

  frame1 = sync_buf;
  frame1_max = sync_buf + pre_frame;

  while (frame1 <= frame1_max)
  {
    LOAD(frame1 - sync_buf + header_size);
    if (parser->parse_header(frame1, &hinfo1))
    {
      ///////////////////////////////////////////////////////////////////////////
      // Search 2nd syncpoint
      // * Known frame size: search next syncpoint after the end of the frame but
      //   not further than scan size (if defined)
      // * Unknown frame size: search next syncpoint after the minimum frame size
      //   up to scan size or maximum frame size if scan size is unspecified.

      if (hinfo1.frame_size)
      {
        frame2 = frame1 + hinfo1.frame_size;
        frame2_max = frame1 + MAX(hinfo1.scan_size, hinfo1.frame_size);
      }
      else
      {
        frame2 = frame1 + min_frame_size;
        frame2_max = frame1 + (hinfo1.scan_size? hinfo1.scan_size: max_frame_size);
      }

      while (frame2 <= frame2_max)
      {
        LOAD(frame2 - sync_buf + header_size);
        if (!parser->compare_headers(frame1, frame2) || !parser->parse_header(frame2, &hinfo2))
        {
          frame2++;
          continue;
        }

        /////////////////////////////////////////////////////////////////////////
        // Search 3rd syncpoint
        // * Known frame size: search next syncpoint after the end of the frame
        //   but not further than scan size (if defined)
        // * Unknown frame size: expect next syncpoint exactly after frame
        //   interval.

        if (hinfo2.frame_size)
        {
          frame3     = frame2 + hinfo2.frame_size;
          frame3_max = frame2 + MAX(hinfo2.scan_size, hinfo2.frame_size);
        }
        else
        {
          frame3     = frame2 + (frame2 - frame1);
          frame3_max = frame2 + (frame2 - frame1);
        }

        while (frame3 <= frame3_max)
        {
          LOAD(frame3 - sync_buf + header_size);
          if (!parser->compare_headers(frame2, frame3) || !parser->parse_header(frame3))
          {
            frame3++;
            continue;
          }

          ///////////////////////////////////////////////////////////////////////
          // Does the data defore the first frame belong to the stream?
          // If not, send it separately from the first frame

          debris = sync_buf;
          debris_size = frame1 - sync_buf;

          size_t temp_frame_size = hinfo.frame_size? hinfo.frame_size: frame2 - frame1;
          if (debris_size > temp_frame_size)
            return true;

          ///////////////////////////////////////////////////////////////////////
          // DONE! Prepare first frame output.

          memcpy(header_buf, frame1, header_size);
          parser->parse_header(header_buf, &hinfo);

          frame = frame1;
          frame_interval = frame2 - frame1;
          frame_size = hinfo.frame_size? hinfo.frame_size: frame_interval;

          in_sync = true;
          new_stream = true;

          frames++;
          return true;
        }

        /////////////////////////////////////////////////////////////////////////
        // No correct 3rd syncpoint found.
        // Continue 2nd syncpoint scanning.

        frame2++;

      } // while (frame2 <= frame2_max)

      /////////////////////////////////////////////////////////////////////////
      // No correct sync sequence found.
      /////////////////////////////////////////////////////////////////////////

    } // if (parser->parse_header(frame1, &hdr))

    frame1++;

  } // while (frame1 <= frame1_max)

  /////////////////////////////////////////////////////////////////////////////
  // Return debris
  // * Sync buffer does not start with a syncpoint
  // * No correct sync sequence found (false sync)
  //
  // Try to locate next syncpoint and return data up to the poistion found
  // as debris.

  uint8_t *pos = frame1 + 1;
  uint8_t *pos_max = sync_buf + sync_data - header_size;
  while (pos <= pos_max)
  {
    if (parser->parse_header(pos))
      break;
    pos++;
  }

  pre_frame = 0;
  debris = sync_buf;
  debris_size = pos - sync_buf;
  return true;
}


bool 
StreamBuffer::load(uint8_t **data, uint8_t *end)
{
  if (!parser)
  {
    *data = end; // avoid endless loop
    return false;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Syncronize with a new stream if we're not in sync

  if (!in_sync)
    return sync(data, end);

  /////////////////////////////////////////////////////////////////////////////
  // Drop old debris and frame data

  if (is_frame_loaded())
  {
    DROP(debris_size + frame_size);
    debris_size = 0;
    frame_size = 0;
  }

  new_stream = false;

  /////////////////////////////////////////////////////////////////////////////
  // Load next frame

  frame = sync_buf;
  uint8_t *frame_max = sync_buf;

  if (hinfo.frame_size)
    if (hinfo.frame_size < hinfo.scan_size)
      frame_max += hinfo.scan_size - hinfo.frame_size;

  while (frame <= frame_max)
  {
    LOAD(frame - sync_buf + header_size);

    HeaderInfo new_hinfo;
    if (!parser->compare_headers(header_buf, frame) || !parser->parse_header(frame, &new_hinfo))
    {
      frame++;
      continue;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Load the rest of the frame

    LOAD(frame - sync_buf + (new_hinfo.frame_size? new_hinfo.frame_size: frame_interval));
      
    ///////////////////////////////////////////////////////////////////////////
    // DONE! Prepare new frame output.

    if (hinfo.frame_size)
      frame_interval = hinfo.frame_size + frame - sync_buf;

    memcpy(header_buf, frame, header_size);
    parser->parse_header(header_buf, &hinfo);

    debris = sync_buf;
    debris_size = frame - sync_buf;

    if (hinfo.frame_size)
      frame_size = hinfo.frame_size;
    else
      frame_size = frame_interval;

    frames++;
    return true;
  }

  /////////////////////////////////////////////////////////////////////////////
  // No correct syncpoint found. Resync.

  in_sync = false;
  pre_frame = max_frame_size;

  frame = 0;
  frame_size = 0;
  frame_interval = 0;
  return sync(data, end);
}

bool 
StreamBuffer::load_frame(uint8_t **data, uint8_t *end)
{
  while (*data < end || is_frame_loaded() || is_debris_exists())
  {
    load(data, end);
    if (is_frame_loaded())
      return true;
  }
  return false;
}

bool
StreamBuffer::flush()
{
  DROP(debris_size + frame_size);
  debris_size = 0;
  frame_size = 0;

  in_sync = false;
  new_stream = false;

  debris = sync_buf;
  debris_size = sync_data;

  return debris_size > 0;
}


size_t
StreamBuffer::stream_info(char *buf, size_t size) const
{
  char info[1024];
  size_t info_size = 0;

  if (parser)
  {
    if (in_sync)
    {
      info_size += parser->header_info(frame, info, sizeof(info));
      info_size += sprintf(info + info_size, "Frame interval: %i\n", frame_interval);
      if (frame_interval > 0 && hinfo.nsamples > 0)
        info_size += sprintf(info + info_size, "Actual bitrate: %ikbps\n", frame_interval * hinfo.spk.sample_rate * 8 / hinfo.nsamples / 1000);
    }
    else
      info_size += sprintf(info + info_size, "Out of sync");
  }
  else
    info_size += sprintf(info + info_size, "No parser set");

  if (info_size > size) info_size = size;
  memcpy(buf, info, info_size);
  buf[info_size] = 0;
  return info_size;
};
