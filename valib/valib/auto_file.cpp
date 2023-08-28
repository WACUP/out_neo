#include <limits.h>
#include "auto_file.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1400)

///////////////////////////////////////////////////////////////////////////////
// Vistual C implementation

const AutoFile::fsize_t AutoFile::max_size = (AutoFile::fsize_t)-1;

static int portable_seek(FILE *f, AutoFile::fsize_t pos, int origin)
{ return _fseeki64(f, pos, origin); }

static AutoFile::fsize_t portable_tell(FILE *f)
{ return _ftelli64(f); }

#else

///////////////////////////////////////////////////////////////////////////////
// Standard Library implementation

const AutoFile::fsize_t AutoFile::max_size = LONG_MAX;

static int portable_seek(FILE *f, AutoFile::fsize_t pos, int origin)
{
  assert(pos < AutoFile::max_size);
  return fseek(f, (long)pos, origin);
}

static AutoFile::fsize_t portable_tell(FILE *f)
{ return ftell(f); }

#endif

///////////////////////////////////////////////////////////////////////////////
// Open/close file

bool
AutoFile::open(const char *filename, const char *mode)
{
  if (f) close();
  filesize = max_size;
  f = fopen(filename, mode);
  if (f)
  {
    if (portable_seek(f, 0, SEEK_END) == 0)
      filesize = portable_tell(f);
    portable_seek(f, 0, SEEK_SET);
    own_file = true;
  }
  return is_open();
}

bool
AutoFile::open(FILE *_f, bool _take_ownership)
{
  if (f) close();
  filesize = max_size;
  f = _f;
  if (f)
  {
    fsize_t old_pos = pos();
    if (portable_seek(f, 0, SEEK_END) == 0)
      filesize = portable_tell(f);
    portable_seek(f, old_pos, SEEK_SET);
    own_file = _take_ownership;
  }
  return is_open();
}

void
AutoFile::close()
{
  if (f && own_file)
    fclose(f);
  f = 0;
}

int
AutoFile::seek(fsize_t _pos)
{
  return portable_seek(f, _pos, SEEK_SET);
}

AutoFile::fsize_t
AutoFile::pos() const
{
  return portable_tell(f);
}

///////////////////////////////////////////////////////////////////////////////

MemFile::MemFile(const char *filename): data(0), file_size(0)
{
  AutoFile f(filename, "rb");
  if (!f.is_open() || f.size() == f.max_size || f.is_large(f.size()))
    return;

  file_size = f.size_cast(f.size());
  data = new uint8_t[file_size];
  if (!data)
    return;

  size_t read_size = f.read(data, file_size);
  if (read_size != file_size)
  {
    safe_delete(data);
    file_size = 0;
    return;
  }
}

MemFile::~MemFile()
{
  safe_delete(data);
}
