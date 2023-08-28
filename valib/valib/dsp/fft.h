/*
  Simple wrapper class for Ooura FFT
*/

#ifndef FFT_H
#define FFT_H

#include "../defs.h"
#include "../auto_buf.h"

class FFT
{
protected:
  AutoBuf<int> fft_ip;
  AutoBuf<sample_t> fft_w;
  unsigned len;

public:
  FFT();
  FFT(unsigned length);

  bool set_length(unsigned length);
  unsigned get_length() const { return len; }
  bool is_ok() const { return fft_ip.is_allocated() && fft_w.is_allocated(); }

  void rdft(sample_t *samples);
  void inv_rdft(sample_t *samples);
};

#endif
