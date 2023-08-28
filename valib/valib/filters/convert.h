/*
  PCM format conversions

  Input formats:   PCMxx, Linear
  Ouptupt formats: PCMxx, Linear
  Format conversions:
    Linear -> PCMxx
    PCMxx  -> Linear
  Default format conversion:
    Linear -> PCM16
  Timing: Preserve original
  Buffering: yes/no
*/

#ifndef VALIB_CONVERT_H
#define VALIB_CONVERT_H

#include "../buffer.h"
#include "../filter.h"
#include "convert_func.h"

///////////////////////////////////////////////////////////////////////////////
// Converter class
///////////////////////////////////////////////////////////////////////////////

class Converter : public NullFilter
{
protected:
  // conversion function pointer
  void (*convert)(uint8_t *, samples_t, size_t);

  // format
  int  format;             // format to convert to
                           // affected only by set_format() function
  int  order[NCHANNELS];   // channel order to convert to when converting from linear format
                           // channel order to convert from when converting to linear format

  // converted samples buffer
  Rawdata   buf;           // buffer for converted data
  size_t    nsamples;      // buffer size in samples

  // output data pointers
  uint8_t  *out_rawdata;   // buffer pointer for pcm data
  samples_t out_samples;   // buffer pointers for linear data
  size_t    out_size;      // buffer size in bytes/samples for pcm/linear data

  // part of sample from previous call
  uint8_t   part_buf[48];  // partial sample left from previous call
  size_t    part_size;     // partial sample size in bytes

  convert_t find_conversion(int _format, Speakers _spk) const;
  bool initialize();       // initialize convertor
  void convert_pcm2linear();
  void convert_linear2pcm();
  bool is_lpcm(int format) { return format == FORMAT_LPCM20 || format == FORMAT_LPCM24; }

public:
  Converter(size_t _nsamples);

  /////////////////////////////////////////////////////////
  // Converter interface

  // buffer size
  size_t get_buffer() const;
  bool   set_buffer(size_t _nsamples);
  // output format
  int  get_format() const;
  bool set_format(int _format);
  // output channel order
  void get_order(int _order[NCHANNELS]) const;
  void set_order(const int _order[NCHANNELS]);

  /////////////////////////////////////////////////////////
  // Filter interface

  virtual void reset();

  virtual bool query_input(Speakers spk) const;
  virtual bool set_input(Speakers spk);
  virtual bool process(const Chunk *chunk);

  virtual Speakers get_output() const;
  virtual bool get_chunk(Chunk *out);
};

#endif
