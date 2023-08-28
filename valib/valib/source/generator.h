/*
  Signal generation sources
  Generator class is an abstract base for generators:
  * Silence generator (ZeroGen)
  * Noise generator (NoiseGen)
  * Sine wave generator (SineGen)
  * Line generator (LinGen)
*/

#ifndef VALIB_GENERATOR_H
#define VALIB_GENERATOR_H

#include "../filter.h"
#include "../buffer.h"
#include "../rng.h"

class Generator : public Source
{
protected:
  Speakers  spk;
  SampleBuf samples;
  Rawdata   rawdata;
  size_t    chunk_size;
  uint64_t  stream_len;

  virtual bool query_spk(Speakers spk) const { return true; }
  virtual void gen_samples(samples_t samples, size_t n) { assert(false); }
  virtual void gen_rawdata(uint8_t *rawdata, size_t n) { assert(false); }

  bool init(Speakers spk, uint64_t stream_len, size_t chunk_size = 4096);

  Generator();
  Generator(Speakers spk, uint64_t stream_len, size_t chunk_size = 4096);

public:
  size_t   get_chunk_size() const { return chunk_size; }
  uint64_t get_stream_len() const { return stream_len; }

  // Source interface
  virtual Speakers get_output() const;
  virtual bool is_empty() const;
  virtual bool get_chunk(Chunk *chunk);
};

class ZeroGen : public Generator
{
protected:
  virtual void gen_samples(samples_t samples, size_t n);
  virtual void gen_rawdata(uint8_t *rawdata, size_t n);

public:
  ZeroGen() {};
  ZeroGen(Speakers _spk, size_t _stream_len, size_t _chunk_size = 4096)
  :Generator(_spk, _stream_len, _chunk_size) {}

  bool init(Speakers _spk, uint64_t _stream_len, size_t _chunk_size = 4096)
  { return Generator::init(_spk, _stream_len, _chunk_size); }
};

class NoiseGen : public Generator
{
protected:
  RNG rng;
  virtual void gen_samples(samples_t samples, size_t n);
  virtual void gen_rawdata(uint8_t *rawdata, size_t n);

public:
  NoiseGen() {};
  NoiseGen(Speakers _spk, int _seed, uint64_t _stream_len, size_t _chunk_size = 4096)
  :Generator(_spk, _stream_len, _chunk_size), rng(_seed) {}

  bool init(Speakers spk, int seed, uint64_t stream_len, size_t chunk_size = 4096);
};

class ToneGen : public Generator
{
protected:
  double phase;
  double freq;

  virtual bool query_spk(Speakers spk) const;
  virtual void gen_samples(samples_t samples, size_t n);
  virtual void gen_rawdata(uint8_t *rawdata, size_t n);

public:
  ToneGen(): phase(0), freq(0) {};
  ToneGen(Speakers _spk, int _freq, double _phase, uint64_t _stream_len, size_t _chunk_size = 4096):
  Generator(_spk, _stream_len, _chunk_size), phase(0), freq(0)
  { init(_spk, _freq, _phase, _stream_len, _chunk_size); }

  bool init(Speakers spk, int freq, double phase, uint64_t stream_len, size_t chunk_size = 4096);
};

class LineGen : public Generator
{
protected:
  double phase;
  double k;

  virtual bool query_spk(Speakers spk) const;
  virtual void gen_samples(samples_t samples, size_t n);
  virtual void gen_rawdata(uint8_t *rawdata, size_t n);

public:
  LineGen(): phase(0), k(1.0) {};
  LineGen(Speakers _spk, double _start, double _k, uint64_t _stream_len, size_t _chunk_size = 4096):
  Generator(_spk, _stream_len, _chunk_size), phase(0), k(1.0)
  { init(_spk, _start, _k, _stream_len, _chunk_size); }

  bool init(Speakers spk, double _start, double _k, uint64_t stream_len, size_t chunk_size = 4096);
};

#endif
