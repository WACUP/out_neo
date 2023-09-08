/*
  WAV file source
*/

#ifndef WAV_SOURCE_H
#define WAV_SOURCE_H

#include "../buffer.h"
#include "../auto_file.h"
#include "../filter.h"

class WAVSource : public Source
{
protected:
  AutoFile f;
  Rawdata  buf;
  Speakers spk;

  size_t block_size;

  AutoFile::fsize_t data_start;
  uint64_t          data_size;
  uint64_t          data_remains;

  bool open_riff();

public:
  WAVSource();
  WAVSource(const char *filename, size_t block_size);

  bool open(const char *filename, size_t block_size);
  void close();
  bool is_open() const;

  AutoFile::fsize_t size() const;
  AutoFile::fsize_t pos() const;
  int seek(AutoFile::fsize_t pos);

  /////////////////////////////////////////////////////////
  // Source interface
  Speakers get_output() const;
  bool is_empty() const;
  bool get_chunk(Chunk *chunk);
};

#endif