#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "file_parser.h"


#define FLOAT_THRESHOLD 1e-20
static const size_t max_buf_size = 65536;

int compact_size(AutoFile::fsize_t size)
{
  int iter = 0;
  while (size >= 10000 && iter < 5)
  {
    size /= 1024;
    iter++;
  }
  return (int)size;
}

const char *compact_suffix(AutoFile::fsize_t size)
{
  static const char *suffixes[] = { "", "K", "M", "G", "T", "P" };

  int iter = 0;
  while (size >= 10000 && iter < 5)
  {
    size /= 1024;
    iter++;
  }
  return suffixes[iter];
}

FileParser::FileParser()
{
  filename = 0;

  buf = new uint8_t[max_buf_size];
  buf_size = buf? max_buf_size: 0;
  buf_data = 0;
  buf_pos = 0;

  stat_size = 0;
  avg_frame_interval = 0;
  avg_bitrate = 0;

  max_scan = 0;
}

FileParser::~FileParser()
{
  close();
  safe_delete(buf);
}

///////////////////////////////////////////////////////////////////////////////
// File operations

bool 
FileParser::open(const char *_filename, const HeaderParser *_parser, size_t _max_scan)
{
  if (!buf || !_parser || !_filename) return false;

  if (is_open()) 
    close();

  if (!stream.set_parser(_parser))
    return false;

  if (!f.open(_filename))
    return false;

  max_scan = _max_scan;
  filename = _strdup(_filename);

  reset();
  return true;
}

void 
FileParser::close()
{
  stream.release_parser();
  f.close();

  safe_delete(filename);

  stat_size = 0;
  avg_frame_interval = 0;
  avg_bitrate = 0;

  max_scan = 0;
}

bool 
FileParser::probe()
{
  if (!f) return false;

  fsize_t old_pos = f.pos();
  bool result = load_frame();
  f.seek(old_pos);
  return result;
}

bool
FileParser::stats(unsigned max_measurments, vtime_t precision)
{
  if (!f) return false;

  fsize_t old_pos = f.pos();

  // If we cannot load a frame we will not gather any stats.
  // (If file format is unknown measurments may take much of time)
  if (!load_frame())
  {
    f.seek(old_pos);
    return false;
  }

  stat_size = 0;
  avg_frame_interval = 0;
  avg_bitrate = 0;

  vtime_t old_length;
  vtime_t new_length;

  old_length = 0;
  for (unsigned i = 0; i < max_measurments; i++)
  {
    fsize_t file_pos = fsize_t((double)rand() * f.size() / RAND_MAX);
    f.seek(file_pos);

    if (!load_frame())
      continue;

    ///////////////////////////////////////////////////////
    // Update stats

    HeaderInfo hinfo = stream.header_info();

    stat_size++;
    avg_frame_interval += stream.get_frame_interval();
    avg_bitrate        += float(stream.get_frame_interval() * 8 * hinfo.spk.sample_rate) / hinfo.nsamples;

    ///////////////////////////////////////////////////////
    // Finish scanning if we have enough accuracy

    if (precision > FLOAT_THRESHOLD)
    {
      new_length = double(f.size()) * 8 * stat_size / avg_bitrate;
      if (stat_size > 10 && fabs(old_length - new_length) < precision)
        break;
      old_length = new_length;
    }
  }

  if (stat_size)
  {
    avg_frame_interval /= stat_size;
    avg_bitrate        /= stat_size;
  }

  seek(old_pos);
  return stat_size > 0;
}

size_t 
FileParser::file_info(char *buf, size_t size) const
{
  char info[1024];

  size_t len = sprintf(info,
    "File: %s\n"
    "Size: %.0f (%i %sB)\n",
    filename,
    (double)f.size(), compact_size(f.size()), compact_suffix(f.size()));

  if (stat_size)
    len += sprintf(info + len,
      "Length: %i:%02i:%02i\n"
      "Frames: %i\n"
      "Frame interval: %i\n"
      "Bitrate: %ikbps\n",
      int(get_size(time)) / 3600, int(get_size(time)) % 3600 / 60, int(get_size(time)) % 60,
      int(get_size(frames)), 
      int(avg_frame_interval),
      int(avg_bitrate / 1000));

  if (len + 1 > size) len = size - 1;
  memcpy(buf, info, len + 1);
  buf[len] = 0;
  return len;
}

///////////////////////////////////////////////////////////////////////////////
// Positioning

inline double
FileParser::units_factor(units_t units) const
{
  switch (units)
  {
    case bytes:    return 1.0;
    case relative: return 1.0 / f.size();
  }

  if (stat_size)
    switch (units)
    {
      case frames:  return 1.0 / avg_frame_interval;
      case time:    return 8.0 / avg_bitrate;
    }

  return 0.0;
}

FileParser::fsize_t
FileParser::get_pos() const
{
  return f.is_open()? fsize_t(f.pos() - buf_data + buf_pos): 0;
}

double 
FileParser::get_pos(units_t units) const
{
  return get_pos() * units_factor(units);
}

FileParser::fsize_t
FileParser::get_size() const
{
  return f.size();
}

double 
FileParser::get_size(units_t units) const
{
  return f.size() * units_factor(units);
}

int
FileParser::seek(fsize_t pos)
{
  int result = f.seek(pos);
  reset();
  return result;
}

int
FileParser::seek(double pos, units_t units)
{ 
  double factor = units_factor(units);
  if (factor > FLOAT_THRESHOLD)
    return seek(fsize_t(pos / factor));
  return -1;
}

///////////////////////////////////////////////////////////////////////////////
// Frame-level interface (StreamBuffer interface wrapper)

void
FileParser::reset()
{
  buf_data = 0;
  buf_pos = 0;
  stream.reset();
}

bool
FileParser::load_frame()
{
  size_t sync_size = 0;

  while (1)
  {
    ///////////////////////////////////////////////////////
    // Load a frame

    uint8_t *pos = buf + buf_pos;
    uint8_t *end = buf + buf_data;
    if (stream.load_frame(&pos, end))
    {
      buf_pos = pos - buf;
      return true;
    }

    ///////////////////////////////////////////////////////
    // Stop file scanning if scanned too much

    sync_size += (pos - buf) - buf_pos;
    buf_pos = pos - buf;
    if (max_scan > 0) // do limiting
    {
      if ((sync_size > stream.get_parser()->max_frame_size() * 3) && // minimum required to sync and load a frame
          (sync_size > max_scan))                                    // limit scanning
        return false;
    }

    ///////////////////////////////////////////////////////
    // Fill the buffer

    if (!buf_data || buf_pos >= buf_data)
    {
      /* Move the data
      if (buf_data && buf_pos)
        memmove(buf, buf + buf_pos, buf_data - buf_pos);
      buf_pos = 0;
      buf_data -= buf_pos;
      */

      buf_pos = 0;
      buf_data = fread(buf, 1, max_buf_size, f);
      if (!buf_data) return false;
    }

  }
  // never be here
  return false;
}
