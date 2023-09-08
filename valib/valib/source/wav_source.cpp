#include <memory.h>
#include "wav_source.h"
#include "../win32/winspk.h"

const uint32_t fcc_riff = be2int32('RIFF');
const uint32_t fcc_rf64 = be2int32('RF64');
const uint32_t fcc_wave = be2int32('WAVE');
const uint32_t fcc_ds64 = be2int32('ds64');
const uint32_t fcc_fmt  = be2int32('fmt ');
const uint32_t fcc_data = be2int32('data');

struct ChunkHeader
{
  uint32_t fcc;
  uint32_t size;
};

struct RIFFChunk
{
  uint32_t fcc;
  uint32_t size;
  uint32_t type;
};

struct DS64Chunk
{
  uint32_t fcc;
  uint32_t size;
  uint64_t riff_size;
  uint64_t data_size;
  uint64_t sample_count;
  uint32_t table_length;
};

struct FMTChunk
{
  uint32_t fcc;
  uint32_t size;
  WAVEFORMATEX wfx;
};

WAVSource::WAVSource()
{
  spk = spk_unknown;
  block_size = 0;
  data_start = 0;
  data_size  = 0;
  data_remains = 0;
}

WAVSource::WAVSource(const char *_filename, size_t _block_size)
{
  spk = spk_unknown;
  block_size = 0;
  data_start = 0;
  data_size  = 0;
  data_remains = 0;

  open(_filename, _block_size);
}

bool WAVSource::open(const char *_filename, size_t _block_size)
{
  close();
  f.open(_filename);
  if (!f.is_open())
    return false;

  if (!open_riff())
  {
    close();
    return false;
  }

  block_size = _block_size;
  if (!buf.allocate(block_size))
  {
    close();
    return false;
  }

  f.seek(data_start);
  data_remains = data_size;

  return true;
}

bool WAVSource::open_riff()
{
  /////////////////////////////////////////////////////////
  // Initializes spk, data_start and data_size
  const size_t buf_size = 256;
  uint8_t buf[buf_size];
  size_t buf_data;

  ChunkHeader *header = (ChunkHeader *)buf;
  RIFFChunk   *riff   = (RIFFChunk *)buf;
  DS64Chunk   *ds64   = (DS64Chunk *)buf;
  FMTChunk    *fmt    = (FMTChunk *)buf;

  /////////////////////////////////////////////////////////
  // Check RIFF header
  f.seek(0);
  buf_data = f.read(buf, sizeof(RIFFChunk));
  if (buf_data < sizeof(RIFFChunk) ||
      (riff->fcc != fcc_riff && riff->fcc != fcc_rf64) || 
      riff->type != fcc_wave)
    return false;

  /////////////////////////////////////////////////////////
  // Chunk walk
  bool have_fmt = false;
  bool have_ds64 = false;
  uint64_t data_size64 = 0;
  AutoFile::fsize_t next = f.pos();
  while (1)
  {
    f.seek(next);
    buf_data = f.read(buf, sizeof(ChunkHeader));
    if (buf_data < sizeof(ChunkHeader))
      return false;

    ///////////////////////////////////////////////////////
    // Format chunk
    if (header->fcc == fcc_fmt)
    {
      if (data_size + header->size > buf_size)
        return false;

      memset(&fmt->wfx, 0, sizeof(fmt->wfx));
      buf_data = f.read(buf + buf_data, header->size);
      if (buf_data < header->size)
        return false;

      if (!wfx2spk(&fmt->wfx, spk))
        return false;

      have_fmt = true;
      //XILASZ commented because it prevent wav playing
	  //break;
    }

    ///////////////////////////////////////////////////////
    // ds64 chunk
    if (header->fcc == fcc_ds64)
    {
      buf_data += f.read(buf + buf_data, sizeof(DS64Chunk) - buf_data);
      if (buf_data < sizeof(DS64Chunk))
        return false;

      data_size64 = ds64->data_size;
      have_ds64 = true;
      break;
    }

    ///////////////////////////////////////////////////////
    // Data chunk
    if (header->fcc == fcc_data)
    {
      if (!have_fmt)
        return false;

      data_start = next + sizeof(ChunkHeader);
      data_size = header->size;
      if (have_ds64 && header->size >= 0xffffff00)
        data_size = data_size64;

      f.seek(data_start);
      data_remains = data_size;
      return true;
    }

    next += header->size + sizeof(ChunkHeader);
  }

  // never be here
  return false;
}

void
WAVSource::close()
{
  spk = spk_unknown;
  block_size = 0;
  data_start = 0;
  data_size  = 0;
  data_remains = 0;

  f.close();
}

bool WAVSource::is_open() const
{
  return f.is_open();
}

AutoFile::fsize_t WAVSource::size() const
{
  if (data_size > (uint64_t)AutoFile::max_size)
    return AutoFile::max_size;
  return (AutoFile::fsize_t) data_size;
}

AutoFile::fsize_t WAVSource::pos() const
{
  return f.pos() - data_start;
}

int WAVSource::seek(AutoFile::fsize_t _pos)
{
  if (_pos < 0) _pos = 0;
  if ((uint64_t)_pos > data_size) _pos = data_size;

  int result = f.seek(_pos + data_start);
  data_remains = data_size - _pos;
  return result;
}

///////////////////////////////////////////////////////////
// Source interface
Speakers WAVSource::get_output() const
{
  return spk;
}

bool WAVSource::is_empty() const
{
  return data_remains == 0;
}

bool WAVSource::get_chunk(Chunk *_chunk)
{
  size_t len = block_size;
  if (data_remains < block_size)
    len = f.size_cast(data_remains);

  size_t data_read = f.read(buf, len);

  if (data_read < len) // eof
    data_remains = 0;
  else
    data_remains -= len;

  _chunk->set_rawdata(spk, buf, data_read, false, 0, data_remains <= 0);
  return true;
}