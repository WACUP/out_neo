/*
  File parser class
*/

#ifndef VALIB_FILE_PARSER_H
#define VALIB_FILE_PARSER_H

#include <stdio.h>
#include "../auto_file.h"
#include "../filter.h"
#include "../parser.h"
#include "../mpeg_demux.h"


class FileParser // : public Source
{
protected:
  StreamBuffer stream;

  AutoFile f;
  char *filename;

  uint8_t *buf;
  size_t buf_size;
  size_t buf_data;
  size_t buf_pos;

  size_t stat_size;         // number of measurments done by stat() call
  float avg_frame_interval; // average frame interval
  float avg_bitrate;        // average bitrate

public:
  typedef AutoFile::fsize_t fsize_t;
  size_t max_scan;
  enum units_t { bytes, relative, frames, time };
  inline double units_factor(units_t units) const;

  FileParser();
  FileParser(const char *filename, const HeaderParser *parser, size_t max_scan = 0);
  ~FileParser();

  /////////////////////////////////////////////////////////////////////////////
  // File operations

  bool open(const char *filename, const HeaderParser *parser, size_t max_scan = 0);
  void close();

  bool probe();
  bool stats(unsigned max_measurments = 100, vtime_t precision = 0.5);

  bool is_open() const { return f != 0; }
  bool eof() const { return f.eof() && (buf_pos >= buf_data) && !stream.is_frame_loaded(); }

  const char *get_filename() const { return filename; }
  const HeaderParser *get_parser() const { return stream.get_parser(); }

  size_t file_info(char *buf, size_t size) const;

  /////////////////////////////////////////////////////////////////////////////
  // Positioning

  fsize_t get_pos() const;
  double  get_pos(units_t units) const;

  fsize_t get_size() const;
  double  get_size(units_t units) const;
  
  int     seek(fsize_t pos);
  int     seek(double pos, units_t units);

  /////////////////////////////////////////////////////////////////////////////
  // Frame-level interface (StreamBuffer interface wrapper)

  void reset();
  bool load_frame();

  bool is_in_sync()             const { return stream.is_in_sync();         }
  bool is_new_stream()          const { return stream.is_new_stream();      }
  bool is_frame_loaded()        const { return stream.is_frame_loaded();    }

  Speakers get_spk()            const { return stream.get_spk();            }
  uint8_t *get_frame()          const { return stream.get_frame();          }
  size_t   get_frame_size()     const { return stream.get_frame_size();     }
  size_t   get_frame_interval() const { return stream.get_frame_interval(); }

  int        get_frames()       const { return stream.get_frames();         }
  size_t     stream_info(char *buf, size_t size) const { return stream.stream_info(buf, size); }
  HeaderInfo header_info()      const { return stream.header_info();        }

  /////////////////////////////////////////////////////////////////////////////
  // Source interface
/*
  virtual Speakers get_output() const;
  virtual bool is_empty() const;
  virtual bool get_chunk(Chunk *chunk);
*/
};

#endif
