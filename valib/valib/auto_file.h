/*
  AutoFile - Simple file helper class. Supports large files >2G.
  MemFile - Load the whole file into memory.

  AutoFile may support large files >2G (currently only in Visual C++). If you
  open a large file without large file support, file size is set to
  max_size constant. In this case you can use the file, but cannot seek
  and get the file position over the max_size limit. Note, that limit is only
  2Gb because position in standard library is signed and may not work
  correctly over this limit.

  VC6 does not support uint64_t to double cast, therefoere we have to use
  the signed type int64_t.

  Note, that file size type, fsize_t may be larger than size_t (it's true on
  32bit system). It's because the file may be larger than memory address space.
  Therefore we cannot safely cast fsize_t to size_t.

  is_large() helps to detect a large value that cannot be cast to size_t.
  size_cast() casts the value to size_t. Returns -1 when the value is too large.
*/

#ifndef VALIB_AUTO_FILE_H
#define VALIB_AUTO_FILE_H

#include "defs.h"
#include <stdio.h>

class AutoFile
{
public:
  typedef int64_t fsize_t;
  static const fsize_t max_size;

  static bool is_large(fsize_t value) { return value > (fsize_t)(size_t)-1; }
  static size_t size_cast(fsize_t value) { return is_large(value)? -1: (size_t)value; }

protected:
  FILE *f;
  bool own_file;
  fsize_t filesize;

public:
  AutoFile(): f(0)
  {}

  AutoFile(const char *filename, const char *mode = "rb"): f(0)
  { open(filename, mode); }

  AutoFile(FILE *_f, bool _take_ownership = false): f(0)
  { open(_f, _take_ownership); }

  ~AutoFile()
  { close(); }

  bool open(const char *filename, const char *mode = "rb");
  bool open(FILE *_f, bool _take_ownership = false);
  void close();

  inline size_t  read(void *buf, size_t size)        { return fread(buf, 1, size, f);  }
  inline size_t  write(const void *buf, size_t size) { return fwrite(buf, 1, size, f); }

  inline bool    is_open()  const { return f != 0;                }
  inline bool    eof()      const { return f? feof(f) != 0: true; }
  inline fsize_t size()     const { return filesize;              }
  inline FILE   *fh()       const { return f;                     }

  inline operator FILE *()  const { return f; }

  int seek(fsize_t _pos);
  fsize_t pos() const;
};


class MemFile
{
protected:
  void *data;
  size_t file_size;

public:
  MemFile(const char *filename);
  ~MemFile();

  inline size_t size() const { return file_size; }
  inline operator uint8_t *() const { return (uint8_t *)data; }
};

#endif
