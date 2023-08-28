/*
  Simple buffer helper class
*/

#ifndef VALIB_AUTO_BUF_H
#define VALIB_AUTO_BUF_H

#include <string.h>
#include "defs.h"

template <class T> class AutoBuf
{
private:
  T *f_buf;
  size_t f_size;
  size_t f_allocated;

  AutoBuf(const AutoBuf &);
  AutoBuf &operator =(const AutoBuf &);

public:
  AutoBuf(): f_buf(0), f_size(0), f_allocated(0)
  {}

  AutoBuf(size_t size): f_buf(0), f_size(0), f_allocated(0)
  {
    allocate(size);
  }

  ~AutoBuf()
  { 
    free();
  }

  inline T *allocate(size_t size)
  {
    if (f_allocated < size)
    {
      free();
      f_buf = new T[size];
      if (f_buf)
      {
        f_size = size;
        f_allocated = size;
      }
    }
    else
      f_size = size;

    return f_buf;
  }

  inline T *reallocate(size_t size)
  {
    if (f_allocated < size)
    {
      T *new_buf = new T[size];
      if (new_buf)
      {
        memcpy(new_buf, f_buf, f_size * sizeof(T));
        safe_delete(f_buf);
        f_buf = new_buf;
        f_size = size;
        f_allocated = size;
      }
      else
        free();
    }
    else
      f_size = size;

    return f_buf;
  }

  inline void free()
  {
    safe_delete(f_buf);
    f_size = 0;
    f_allocated = 0;
  }

  inline void zero()
  {
    if (f_buf)
      memset(f_buf, 0, f_size * sizeof(T));
  }

  inline size_t size() const { return f_size; }
  inline size_t allocated() const { return f_allocated; }
  inline T *data() const { return f_buf;  }
  inline operator T*() const { return f_buf;  }
  inline bool is_allocated() const { return f_buf != 0; }
};

#endif
