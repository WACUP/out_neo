#ifndef VALIB_CONVOLVER_MCH_H
#define VALIB_CONVOLVER_MCH_H

#include <math.h>
#include "linear_filter.h"
#include "../fir.h"
#include "../sync.h"
#include "../buffer.h"
#include "../dsp/fft.h"


///////////////////////////////////////////////////////////////////////////////
// Multichannel convolver class
// Use impulse response to implement FIR filtering.
///////////////////////////////////////////////////////////////////////////////

class ConvolverMch : public LinearFilter
{
protected:
  int ver[NCHANNELS];
  FIRRef gen[NCHANNELS];

  bool trivial;
  const FIRInstance *fir[NCHANNELS];
  enum { type_pass, type_gain, type_zero, type_conv } type[NCHANNELS];

  int buf_size;
  int n, c;
  int pos;

  FFT       fft;
  SampleBuf filter;
  SampleBuf buf;
  Samples   fft_buf;

  int pre_samples;
  int post_samples;

  bool fir_changed() const;
  void uninit();

  void process_trivial(samples_t samples, size_t size);
  void process_convolve();

public:
  ConvolverMch();
  ~ConvolverMch();

  /////////////////////////////////////////////////////////
  // Handle FIR generator changes

  void set_fir(int ch_name, const FIRGen *gen);
  const FIRGen *get_fir(int ch_name) const;
  void release_fir(int ch_name);

  void set_all_firs(const FIRGen *gen[NCHANNELS]);
  void get_all_firs(const FIRGen *gen[NCHANNELS]);
  void release_all_firs();

  /////////////////////////////////////////////////////////
  // Filter interface

  virtual bool init(Speakers spk, Speakers &out_spk);
  virtual void reset_state();

  virtual bool process_samples(samples_t in, size_t in_size, samples_t &out, size_t &out_size, size_t &gone);
  virtual bool flush(samples_t &out, size_t &out_size);

  virtual bool need_flushing() const;
};

#endif
