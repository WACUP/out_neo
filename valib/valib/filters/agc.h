/*
  Automatic Gain Control filter
  todo: remove master gain?

  Speakers: unchanged
  Input formats: Linear
  Output formats: Linear
  Buffer: +
  Inline: -
  Delay: nsamples
  Timing: unchanged
  Paramters:
    buffer       // processing buffer length in samples [offline]
    auto_gain    // automatic gain control [online]
    normalize    // one-pass normalize [online]
    master       // desired gain [online]
    gain         // current gain [online]
    release      // release speed (dB/s) [online]
    drc          // DRC enabled [online]
    drc_power    // DRC power (dB) [online]
    drc_level    // current DRC gain level (read-only) [read-only]
*/

#ifndef VALIB_AGC_H
#define VALIB_AGC_H

#include "../buffer.h"
#include "../filter.h"

///////////////////////////////////////////////////////////////////////////////
// AGC class
///////////////////////////////////////////////////////////////////////////////

class AGC : public NullFilter
{
protected:
  SampleBuf w;
  SampleBuf buf[2];               // sample buffers

  size_t    block;                // current block
  size_t    sample[2];            // number of samples filled
  bool      buf_sync[2];          // beginning of the buffer is syncpoint
  vtime_t   buf_time[2];          // timestamp at beginning of the buffer

  size_t    nsamples;             // number of samples per block

  sample_t  factor;               // previous block factor
  sample_t  level;                // previous block level (not scaled)

  inline size_t next_block();

  bool fill_buffer();
  void process();

public:
  // Options
  bool auto_gain;                 // [rw] automatic gain control
  bool normalize;                 // [rw] one-pass normalize

  // Gain control
  sample_t master;                // [rw] desired gain
  sample_t gain;                  // [r]  current gain

  sample_t attack;                // [rw] attack speed (dB/s)
  sample_t release;               // [rw] release speed (dB/s)

  // DRC
  bool     drc;                   // [rw] DRC enabled
  sample_t drc_power;             // [rw] DRC power (dB)
  sample_t drc_level;             // [r]  current DRC gain level (read-only)

  AGC(size_t nsamples);

  /////////////////////////////////////////////////////////
  // AGC interface

  // buffer size
  size_t get_buffer() const;
  void   set_buffer(size_t nsamples);

  /////////////////////////////////////////////////////////
  // Filter interface

  virtual void reset();
  virtual bool get_chunk(Chunk *out);
};


///////////////////////////////////////////////////////////////////////////////
// AGC inlines
///////////////////////////////////////////////////////////////////////////////

size_t 
AGC::next_block()
{
  return (block + 1) & 1;
}

#endif
