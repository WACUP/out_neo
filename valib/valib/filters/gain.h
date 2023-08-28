/*
  Simple gain filter
*/

#ifndef VALIB_GAIN_H
#define VALIB_GAIN_H

#include "../filter.h"

class Gain : public NullFilter
{
public:
  double gain;

  Gain(): NullFilter(FORMAT_MASK_LINEAR) {};
  Gain(double gain_): NullFilter(FORMAT_MASK_LINEAR), gain(gain_) {}

protected:
  virtual bool on_process()
  {
    if (gain != 1.0)
      for (int ch = 0; ch < spk.nch(); ch++)
        for (size_t s = 0; s < size; s++)
          samples[ch][s] *= gain;
    return true;
  }
};

#endif
