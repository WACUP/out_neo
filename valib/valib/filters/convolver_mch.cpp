// TODO!!!
// * use simple convolution for short filters (up to ~32 taps)
//   (FFT filtering is less effective for such lengths)

#include <string.h>
#include "convolver_mch.h"

static const int min_fft_size = 16;
static const int min_chunk_size = 1024;

inline unsigned int clp2(unsigned int x)
{
  // smallest power-of-2 >= x
  x = x - 1;
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  return x + 1;
}


ConvolverMch::ConvolverMch():
  buf_size(0), n(0), c(0),
  pos(0), pre_samples(0), post_samples(0)
{
  for (int ch_name = 0; ch_name < NCHANNELS; ch_name++)
    ver[ch_name] = gen[ch_name].version();

  for (int ch = 0; ch < NCHANNELS; ch++)
  {
    fir[ch] = 0;
    type[ch] = type_pass;
  }
}

ConvolverMch::~ConvolverMch()
{
  uninit();
}

bool
ConvolverMch::fir_changed() const
{
  for (int ch_name = 0; ch_name < NCHANNELS; ch_name++)
    if (ver[ch_name] != gen[ch_name].version())
      return true;
  return false;
}

///////////////////////////////////////////////////////////////////////////////

void
ConvolverMch::set_fir(int ch_name, const FIRGen *new_gen)
{
  gen[ch_name] = new_gen;
  reinit(false);
}

const FIRGen *
ConvolverMch::get_fir(int ch_name) const
{
  return gen[ch_name].get();
}

void
ConvolverMch::release_fir(int ch_name)
{
  gen[ch_name].release();
  reinit(false);
}

void
ConvolverMch::set_all_firs(const FIRGen *new_gen[NCHANNELS])
{
  for (int ch_name = 0; ch_name < NCHANNELS; ch_name++)
    gen[ch_name] = new_gen[ch_name];
  reinit(false);
}

void
ConvolverMch::get_all_firs(const FIRGen *out_gen[NCHANNELS])
{
  for (int ch_name = 0; ch_name < NCHANNELS; ch_name++)
    out_gen[ch_name] = gen[ch_name].get();
}

void
ConvolverMch::release_all_firs()
{
  for (int ch_name = 0; ch_name < NCHANNELS; ch_name++)
    gen[ch_name].release();
  reinit(false);
}

///////////////////////////////////////////////////////////////////////////////

void
ConvolverMch::process_trivial(samples_t samples, size_t size)
{
  size_t s;
  sample_t gain;

  for (int ch = 0; ch < get_in_spk().nch(); ch++)
    switch (type[ch])
    {
      case type_zero:
        memset(samples[ch], 0, size * sizeof(sample_t));
        break;

      case type_gain:
        gain = fir[ch]->data[0];
        for (s = 0; s < size; s++)
          samples[ch][s] *= gain;
        break;
    }
}

void
ConvolverMch::process_convolve()
{
  int ch, i;
  int nch = get_in_spk().nch();
  sample_t *buf_ch, *filter_ch, *delay_ch;

  for (ch = 0; ch < nch; ch++)
    if (type[ch] == type_conv)
      for (int fft_pos = 0; fft_pos < buf_size; fft_pos += n)
      {
        buf_ch = buf[ch] + fft_pos;
        delay_ch = buf[ch] + buf_size;
        filter_ch = filter[ch];

        memcpy(fft_buf, buf_ch, n * sizeof(sample_t));
        memset(fft_buf + n, 0, n * sizeof(sample_t));

        fft.rdft(fft_buf);

        fft_buf[0] = filter_ch[0] * fft_buf[0];
        fft_buf[1] = filter_ch[1] * fft_buf[1]; 

        for (i = 1; i < n; i++)
        {
          sample_t re,im;
          re = filter_ch[i*2  ] * fft_buf[i*2] - filter_ch[i*2+1] * fft_buf[i*2+1];
          im = filter_ch[i*2+1] * fft_buf[i*2] + filter_ch[i*2  ] * fft_buf[i*2+1];
          fft_buf[i*2  ] = re;
          fft_buf[i*2+1] = im;
        }

        fft.inv_rdft(fft_buf);

        for (i = 0; i < n; i++)
          buf_ch[i] = fft_buf[i] + delay_ch[i];

        memcpy(delay_ch, fft_buf + n, n * sizeof(sample_t));
      }
}

bool ConvolverMch::init(Speakers new_in_spk, Speakers &new_out_spk)
{
  int i, ch, ch_name;
  int nch = new_in_spk.nch();

  trivial = true;
  int min_point = 0;
  int max_point = 0;

  // Update versions
  for (ch_name = 0; ch_name < NCHANNELS; ch_name++)
    ver[ch_name] = gen[ch_name].version();

  for (ch = 0; ch < nch; ch++)
  {
    ch_name = get_in_spk().order()[ch];
    fir[ch] = gen[ch_name].make(new_in_spk.sample_rate);

    // fir generation error
    if (!fir[ch])
    {
      type[ch] = type_pass;
      continue;
    }

    // validate fir instance
    if (fir[ch]->length <= 0 || fir[ch]->center < 0)
    {
      type[ch] = type_pass;
      safe_delete(fir[ch]);
      continue;
    }

    switch (fir[ch]->type)
    {
      case firt_identity: type[ch] = type_pass; break;
      case firt_zero:     type[ch] = type_zero; break;
      case firt_gain:     type[ch] = type_gain; break;
      default:
        type[ch] = type_conv;
        trivial = false;
        break;
    }

    if (min_point > - fir[ch]->center)
      min_point = -fir[ch]->center;
    if (max_point < fir[ch]->length - fir[ch]->center)
      max_point = fir[ch]->length - fir[ch]->center;
  }

  if (trivial)
    return true;

  /////////////////////////////////////////////////////////
  // Allocate buffers

  n = clp2(max_point - min_point);
  c = -min_point;

  if (n < min_fft_size / 2)
    n = min_fft_size / 2;

  buf_size = n;
  if (buf_size < min_chunk_size)
    buf_size = clp2(min_chunk_size);

  fft.set_length(n * 2);
  filter.allocate(nch, n * 2);
  buf.allocate(nch, buf_size + n);
  fft_buf.allocate(n * 2);

  // handle buffer allocation error
  if (!filter.is_allocated() ||
      !buf.is_allocated() ||
      !fft_buf.is_allocated() ||
      !fft.is_ok())
  {
    uninit();
    return false;
  }

  /////////////////////////////////////////////////////////
  // Build filters

  filter.zero();
  for (ch = 0; ch < nch; ch++)
    if (type[ch] == type_conv)
    {
      for (i = 0; i < fir[ch]->length; i++)
        filter[ch][i + c - fir[ch]->center] = fir[ch]->data[i] / n;
      fft.rdft(filter[ch]);
    }

  /////////////////////////////////////////////////////////
  // Initial state

  pos = 0;
  pre_samples = c;
  post_samples = n - c;
  buf.zero();
  return true;
}

void
ConvolverMch::uninit()
{
  buf_size = 0;
  n = 0;
  c = 0;
  pos = 0;

  trivial = true;
  for (int ch = 0; ch < get_in_spk().nch(); ch++)
  {
    safe_delete(fir[ch]);
    type[ch] = type_pass;
  }

  pre_samples = 0;
  post_samples = 0;
}

void
ConvolverMch::reset_state()
{
  pos = 0;
  pre_samples = c;
  post_samples = n - c;
  buf.zero();
}

bool
ConvolverMch::process_samples(samples_t in, size_t in_size, samples_t &out, size_t &out_size, size_t &gone)
{
  int ch;
  int nch = get_in_spk().nch();

  /////////////////////////////////////////////////////////
  // Handle FIR change

  if (fir_changed())
    reinit(false);

  /////////////////////////////////////////////////////////
  // Trivial filtering

  if (trivial)
  {
    process_trivial(in, in_size);

    out = in;
    out_size = in_size;
    gone = in_size;
    return true;
  }

  /////////////////////////////////////////////////////////
  // Convolution

  // Trivial cases:
  // Copy delayed samples to the start of the buffer
  if (pos == 0)
    for (ch = 0; ch < nch; ch++)
      if (type[ch] != type_conv)
        memcpy(buf[ch], buf[ch] + buf_size, c * sizeof(sample_t));

  // Accumulate the buffer
  if (pos < buf_size)
  {
    gone = MIN(in_size, size_t(buf_size - pos));
    for (ch = 0; ch < nch; ch++)
      if (type[ch] == type_conv)
        memcpy(buf[ch] + pos, in[ch], gone * sizeof(sample_t));
      else
        // Trivial cases are shifted
        memcpy(buf[ch] + c + pos, in[ch], gone * sizeof(sample_t));
    pos += (int)gone;

    if (pos < buf_size)
      return true;
  }

  pos = 0;
  process_trivial(buf, buf_size);
  process_convolve();

  out = buf;
  out_size = buf_size;
  if (pre_samples)
  {
    out += pre_samples;
    out_size -= pre_samples;
    pre_samples = 0;
  }

  return true;
}

bool
ConvolverMch::flush(samples_t &out, size_t &out_size)
{
  int ch, nch = get_in_spk().nch();
  if (!need_flushing())
    return true;

  if (pos == 0)
    for (ch = 0; ch < nch; ch++)
      if (type[ch] != type_conv)
        memcpy(buf[ch], buf[ch] + buf_size, c * sizeof(sample_t));

  for (ch = 0; ch < nch; ch++)
    if (type[ch] == type_conv)
      memset(buf[ch] + pos, 0, (buf_size - pos) * sizeof(sample_t));

  process_trivial(buf, buf_size);
  process_convolve();

  out = buf;
  out_size = pos + c;
  post_samples = 0;
  pos = 0;

  if (pre_samples)
  {
    out += pre_samples;
    out_size -= pre_samples;
    pre_samples = 0;
  }
  return true;
}

bool
ConvolverMch::need_flushing() const
{
  return !trivial && post_samples > 0;
}
