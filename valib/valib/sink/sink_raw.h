/*
  RAW file output audio renderer
*/

#ifndef VALIB_SINK_RAW_H
#define VALIB_SINK_RAW_H

#include "filter.h"
#include "auto_file.h"

class RAWSink : public Sink
{
protected:
  Speakers spk;
  AutoFile f;

public:
  RAWSink() 
  {}

  RAWSink(const char *_filename): 
  f(_filename, "wb") 
  {}

  RAWSink(FILE *_f): 
  f(_f) 
  {}

  /////////////////////////////////////////////////////////
  // RAWSink interface

  bool open(const char *_filename)
  {
    return f.open(_filename, "wb");
  }

  bool open(FILE *_f)
  {
    return f.open(_f);
  }

  void close()
  {
    f.close();
    spk = spk_unknown;
  }

  bool is_open() const
  {
    return f.is_open();
  }

  /////////////////////////////////////////////////////////
  // Sink interface

  virtual bool query_input(Speakers _spk) const  
  { 
    // cannot write linear format
    return f.is_open() && _spk.format != FORMAT_LINEAR;
  }

  virtual bool set_input(Speakers _spk)
  { 
    if (!query_input(_spk))
      return false;

    spk = _spk;
    return true;
  }

  virtual Speakers get_input() const
  {
    return spk;
  }

  // data write
  virtual bool process(const Chunk *_chunk)               
  {
    if (_chunk->is_dummy())
      return true;

    if (spk != _chunk->spk)
      if (!set_input(_chunk->spk))
        return false;

    return f.write(_chunk->rawdata, _chunk->size) == _chunk->size;
  }
};

#endif
