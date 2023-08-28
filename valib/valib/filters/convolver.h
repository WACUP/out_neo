#ifndef VALIB_CONVOLVER_H
#define VALIB_CONVOLVER_H

#include <math.h>
#include "linear_filter.h"
#include "../fir.h"
#include "../sync.h"
#include "../buffer.h"
#include "../dsp/fft.h"


///////////////////////////////////////////////////////////////////////////////
// Convolver class
// Use impulse response to implement FIR filtering.
///////////////////////////////////////////////////////////////////////////////

class Convolver : public LinearFilter
{
protected:
  int ver;
  FIRRef gen;
  const FIRInstance *fir;
  SyncHelper sync_helper;

  int buf_size;
  int n, c;
  int pos;

  FFT       fft;
  Samples   filter;
  SampleBuf buf;
  Samples   fft_buf;

  int pre_samples;
  int post_samples;

  bool fir_changed() const;
  void uninit();
  void convolve();

  enum { state_filter, state_zero, state_pass, state_gain } state;

public:
  Convolver(const FIRGen *gen_ = 0);
  ~Convolver();

  /////////////////////////////////////////////////////////
  // Handle FIR generator changes

  void set_fir(const FIRGen *gen_) { gen.set(gen_); reinit(false); }
  const FIRGen *get_fir() const    { return gen.get(); }
  void release_fir()               { gen.release(); reinit(false); }

  /////////////////////////////////////////////////////////
  // Filter interface

  virtual bool init(Speakers spk, Speakers &out_spk);
  virtual void reset_state();

  virtual bool process_samples(samples_t in, size_t in_size, samples_t &out, size_t &out_size, size_t &gone);
  virtual bool flush(samples_t &out, size_t &out_size);

  virtual bool need_flushing() const;
};

#endif
