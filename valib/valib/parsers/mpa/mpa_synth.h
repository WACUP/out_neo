/*
  Synthesis filter classes

  Implements synthesis filter for 
  MPEG1 Audio LayerI and LayerII
*/

#ifndef VALIB_MPA_SYNTH_H
#define VALIB_MPA_SYNTH_H

#include "../../defs.h"

class SynthBuffer;
class SynthBufferFPU;     
//class SynthBufferMMX;     
//class SynthBufferSSE;
//class SynthBufferSSE2;
//class SynthBuffer3DNow;
//class SynthBuffer3DNow2;


///////////////////////////////////////////////////////////
// Synthesis filter interface

class SynthBuffer
{
public:
  virtual void synth(sample_t samples[32]) = 0;
  virtual void reset() = 0;
};


///////////////////////////////////////////////////////////
// FPU synthesis filter

class SynthBufferFPU: public SynthBuffer
{
protected:
  sample_t synth_buf[1024];  // Synthesis buffer
  int      synth_offset;     // Offset in synthesis buffer

public:
  SynthBufferFPU();

  virtual void synth(sample_t samples[32]);
  virtual void reset();
};

#endif
