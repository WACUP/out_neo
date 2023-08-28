#include "multi_header.h"

MultiHeader::MultiHeader()
{
  parsers = 0;
  nparsers = 0;

  f_header_size = 0;
  f_min_frame_size = 0;
  f_max_frame_size = 0;
}

MultiHeader::MultiHeader(const HeaderParser *const *_parsers, size_t _nparsers)
{
  parsers = 0;
  nparsers = 0;

  f_header_size = 0;
  f_min_frame_size = 0;
  f_max_frame_size = 0;

  set_parsers(_parsers, _nparsers);
}

MultiHeader::MultiHeader(const FrameParser *const *_parsers, size_t _nparsers)
{
  parsers = 0;
  nparsers = 0;

  f_header_size = 0;
  f_min_frame_size = 0;
  f_max_frame_size = 0;

  set_parsers(_parsers, _nparsers);
}

MultiHeader::~MultiHeader()
{
  release_parsers();
}

bool
MultiHeader::set_parsers(const HeaderParser *const *_parsers, size_t _nparsers)
{
  release_parsers();

  parsers = new const HeaderParser *[_nparsers];
  if (!parsers)
    return false;

  nparsers = _nparsers;
  for (size_t i = 0; i < _nparsers; i++)
  {
    parsers[i] = _parsers[i];

    if (i == 0)
    {
      f_header_size = parsers[0]->header_size();
      f_min_frame_size = parsers[0]->min_frame_size();
      f_max_frame_size = parsers[0]->max_frame_size();
    }
    else
    {
      if (f_header_size < parsers[i]->header_size())
        f_header_size = parsers[i]->header_size();

      if (f_min_frame_size > parsers[i]->min_frame_size())
        f_min_frame_size = parsers[i]->min_frame_size();

      if (f_max_frame_size < parsers[i]->max_frame_size())
        f_max_frame_size = parsers[i]->max_frame_size();
    }
  }

  if (f_min_frame_size < f_header_size)
    f_min_frame_size = f_header_size;

  return true;
}

bool
MultiHeader::set_parsers(const FrameParser *const *_parsers, size_t _nparsers)
{
  release_parsers();

  parsers = new const HeaderParser *[_nparsers];
  if (!parsers)
    return false;

  nparsers = _nparsers;
  for (size_t i = 0; i < _nparsers; i++)
  {
    parsers[i] = _parsers[i]->header_parser();
    
    if (i == 0)
    {
      f_header_size = parsers[0]->header_size();
      f_min_frame_size = parsers[0]->min_frame_size();
      f_max_frame_size = parsers[0]->max_frame_size();
    }
    else
    {
      if (f_header_size < parsers[i]->header_size())
        f_header_size = parsers[i]->header_size();

      if (f_min_frame_size > parsers[i]->min_frame_size())
        f_min_frame_size = parsers[i]->min_frame_size();

      if (f_max_frame_size < parsers[i]->max_frame_size())
        f_max_frame_size = parsers[i]->max_frame_size();
    }
  }

  if (f_min_frame_size < f_header_size)
    f_min_frame_size = f_header_size;

  return true;
}

void
MultiHeader::release_parsers()
{
  safe_delete(parsers)
  nparsers = 0;

  f_header_size = 0;
  f_min_frame_size = 0;
  f_max_frame_size = 0;
}

bool
MultiHeader::can_parse(int format) const
{
  for (size_t i = 0; i < nparsers; i++)
    if (parsers[i]->can_parse(format))
      return true;
  return false;
}

bool
MultiHeader::parse_header(const uint8_t *hdr, HeaderInfo *hinfo) const
{
  for (size_t i = 0; i < nparsers; i++)
    if (parsers[i]->parse_header(hdr, hinfo))
      return true;
  return false;
}

bool
MultiHeader::compare_headers(const uint8_t *hdr1, const uint8_t *hdr2) const
{
  for (size_t i = 0; i < nparsers; i++)
    if (parsers[i]->compare_headers(hdr1, hdr2))
      return true;
  return false;
}

size_t
MultiHeader::header_info(const uint8_t *hdr, char *buf, size_t size) const
{
  for (size_t i = 0; i < nparsers; i++)
    if (parsers[i]->parse_header(hdr))
      return parsers[i]->header_info(hdr, buf, size);
  return 0;
}
