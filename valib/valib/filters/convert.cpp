#include <math.h>
#include "convert.h"

// todo: PCM-to-PCM conversions

static const int converter_formats = FORMAT_MASK_LINEAR | FORMAT_MASK_PCM16 | FORMAT_MASK_PCM24 | FORMAT_MASK_PCM32 | FORMAT_MASK_PCM16_BE | FORMAT_MASK_PCM24_BE | FORMAT_MASK_PCM32_BE | FORMAT_MASK_PCMFLOAT | FORMAT_MASK_PCMDOUBLE | FORMAT_MASK_LPCM20 | FORMAT_MASK_LPCM24;
static void passthrough(uint8_t *, samples_t, size_t) {}

Converter::Converter(size_t _nsamples)
:NullFilter(0) // use own query_input()
{
  convert = 0;
  format = FORMAT_UNKNOWN;
  memcpy(order, std_order, sizeof(order));
  nsamples = _nsamples;
  out_size = 0;
  part_size = 0;
}

convert_t 
Converter::find_conversion(int _format, Speakers _spk) const
{
  if (_spk.format == _format)
    // no conversion required but we have to return conversion 
    // function to indicate that we can proceed
    return passthrough;

  if (_format == FORMAT_LINEAR)
    return find_pcm2linear(_spk.format, _spk.nch());
  else if (_spk.format == FORMAT_LINEAR)
    return find_linear2pcm(_format, _spk.nch());

  return 0;
}

bool 
Converter::initialize()
{
  /////////////////////////////////////////////////////////
  // Initialize convertor:
  // * reset filter state
  // * find conversion function
  // * allocate buffer
  //
  // If we cannot find conversion we have to drop current 
  // input format to spk_unknown to indicate that we cannot
  // proceed with current setup so forcing application to 
  // call set_input() with new input format.

  /////////////////////////////////////////////////////////
  // reset filter state

  reset();

  /////////////////////////////////////////////////////////
  // check if no conversion required

  if (spk.is_unknown())
  {
    // input format is not set
    convert = passthrough;
    return true; 
  }

  if (spk.format == format)
  {
    // no conversion required
    // no buffer required
    convert = passthrough;
    return true; 
  }
  
  /////////////////////////////////////////////////////////
  // find conversion function

  convert = find_conversion(format, spk);
  if (convert == 0)
  {
    spk = spk_unknown;
    return false;
  }

  /////////////////////////////////////////////////////////
  // allocate buffer

  if (!buf.allocate(spk.nch() * nsamples * sample_size(format)))
  {
    convert = 0;
    spk = spk_unknown;
    return false;
  }

  if (format == FORMAT_LINEAR)
  {
    // set channel pointers
    out_samples[0] = (sample_t *)buf.data();
    for (int ch = 1; ch < spk.nch(); ch++)
      out_samples[ch] = out_samples[ch-1] + nsamples;
    out_rawdata = 0;
  }
  else
  {
    // set rawdata pointer
    out_rawdata = buf.data();
    out_samples.zero();
  }

  return true;
}

void
Converter::convert_pcm2linear()
{
  const size_t sample_size = spk.sample_size() * spk.nch();

  samples_t dst = out_samples;
  out_size = 0;

  /////////////////////////////////////////////////////////
  // Process part of a sample

  if (part_size)
  {
    assert(part_size < sample_size);
    size_t delta = sample_size - part_size;
    if (size < delta)
    {
      // not enough data to fill sample buffer
      memcpy(part_buf + part_size, rawdata, size);
      part_size += size;
      size = 0;
      return;
    }
    else
    {
      // fill the buffer & convert incomplete sample 
      memcpy(part_buf + part_size, rawdata, delta);
      drop_rawdata(delta);
      part_size = 0;

      convert(part_buf, dst, 1);

      dst += 1;
      out_size++;
      if (is_lpcm(spk.format))
      {
        dst += 1;
        out_size++;
      }
    }   
  }

  /////////////////////////////////////////////////////////
  // Determine the number of samples to convert and
  // the size of raw data required for conversion
  // Note: LPCM sample size is doubled because one block
  // contains 2 samples. Also, decoding is done in 2 sample
  // blocks, thus we have to specify less cycles.

  size_t n = nsamples - out_size;
  if (is_lpcm(spk.format)) n /= 2;
  size_t n_size = n * sample_size;
  if (n_size > size)
  {
    n = size / sample_size;
    n_size = n * sample_size;
  }

  /////////////////////////////////////////////////////////
  // Convert

  convert(rawdata, dst, n);

  drop_rawdata(n_size);
  out_size += n;
  if (is_lpcm(spk.format))
    out_size += n;

  /////////////////////////////////////////////////////////
  // Remember the remaining part of a sample

  if (size && size < sample_size)
  {
    memcpy(part_buf, rawdata, size);
    part_size = size;
    size = 0;
  }
}

void
Converter::convert_linear2pcm()
{
  const size_t sample_size = ::sample_size(format) * spk.nch();
  size_t n = MIN(size, nsamples);

  convert(out_rawdata, samples, n);

  drop_samples(n);
  out_size = n * sample_size;
}

///////////////////////////////////////////////////////////
// Converter interface

size_t 
Converter::get_buffer() const
{
  return nsamples;
}

bool 
Converter::set_buffer(size_t _nsamples)
{
  nsamples = _nsamples;
  return initialize();
}

int 
Converter::get_format() const
{
  return format;
}

bool 
Converter::set_format(int _format)
{
  if ((FORMAT_MASK(_format) & converter_formats) == 0)
    return false;

  format = _format;
  return initialize();
}

void 
Converter::get_order(int _order[NCHANNELS]) const
{
  memcpy(_order, order, sizeof(order));
}

void 
Converter::set_order(const int _order[NCHANNELS])
{
  if (format == FORMAT_LINEAR)
    out_samples.reorder(spk, order, _order);
  memcpy(order, _order, sizeof(order));
}

///////////////////////////////////////////////////////////
// Filter interface

void
Converter::reset()
{
  NullFilter::reset();
  part_size = 0;
}

bool 
Converter::query_input(Speakers _spk) const
{
  if ((FORMAT_MASK(_spk.format) & converter_formats) == 0)
    return false;

  if (_spk.nch() == 0)
    return false;

  if (find_conversion(format, _spk) == 0)
    return false;

  return true;
}

bool 
Converter::set_input(Speakers _spk)
{
  if (!NullFilter::set_input(_spk))
    return false;

  return initialize();
}

Speakers 
Converter::get_output() const
{
  if (convert == 0)
    return spk_unknown;

  Speakers out = spk;
  out.format = format;
  return out;
}

bool 
Converter::process(const Chunk *_chunk)
{
  // we must ignore dummy chunks
  if (_chunk->is_dummy())
    return true;

  FILTER_SAFE(receive_chunk(_chunk));

  if (spk.format == FORMAT_LINEAR)
    samples.reorder_from_std(spk, order);

  return true;
}

bool 
Converter::get_chunk(Chunk *_chunk)
{
  const Speakers out_spk = get_output();

  if (spk.format == format)
  {
    send_chunk_inplace(_chunk, size);
    return true;
  }

  if (convert == 0)
    return false;

  if (format == FORMAT_LINEAR)
    convert_pcm2linear();
  else
    convert_linear2pcm();

  _chunk->set
  (
    out_spk, 
    out_rawdata, out_samples, out_size, 
    sync, time, 
    flushing && !size
  );

  if (out_spk.is_linear())
    _chunk->samples.reorder_to_std(spk, order);

  flushing = flushing && size;
  sync = false;
  time = 0;
  return true;
}
