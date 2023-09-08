#include <math.h>
#include "fft.h"
#include "fftsg.h"

MM_FFT::MM_FFT()
{}

MM_FFT::MM_FFT(unsigned length)
{
  set_length(length);
}

bool
MM_FFT::set_length(unsigned length)
{
  if (len == length && is_ok())
    return true;

  len = length;
  fft_ip.allocate((int)(2 + sqrt(double(length * 2))));
  fft_w.allocate(length/2+1);

  if (fft_ip.is_allocated())
    fft_ip[0] = 0;

  return is_ok();
}

void
MM_FFT::rdft(sample_t *samples)
{
  ::rdft(len, 1, samples, fft_ip, fft_w);
}

void
MM_FFT::inv_rdft(sample_t *samples)
{
  ::rdft(len, -1, samples, fft_ip, fft_w);
}
