/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef _OMX_READER_H_
#define _OMX_READER_H_

#include "DllAvFormat.h"
#include "OMXStreamInfo.h"
#include "OMXThread.h"
#include "utils/simple_geometry.h"

#include <sys/types.h>
#include <string>
#include <cassert>

class OMXDvdPlayer;

using namespace std;

#define MAX_OMX_CHAPTERS 64

#define MAX_OMX_STREAMS        100

#ifndef FFMPEG_FILE_BUFFER_SIZE
#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg
#endif
#ifndef MAX_STREAMS
#define MAX_STREAMS 100
#endif

class OMXPacket : public AVPacket
{
  public: 
  OMXPacket();
  ~OMXPacket();
  
  COMXStreamInfo hints;
  enum AVMediaType codec_type;
};

enum OMXStreamType
{
  OMXSTREAM_NONE      = 0,
  OMXSTREAM_AUDIO     = 1,
  OMXSTREAM_VIDEO     = 2,
  OMXSTREAM_SUBTITLE  = 3
};

typedef struct OMXStream
{
  char language[4];
  std::string name;
  std::string codec_name;
  AVStream    *stream;
  OMXStreamType type;
  int         id;
  void        *extradata;
  unsigned int extrasize;
  unsigned int index;
  COMXStreamInfo hints;
} OMXStream;

class OMXReader
{
protected:
  int                       m_video_index;
  int                       m_audio_index;
  int                       m_subtitle_index;
  int                       m_video_count;
  int                       m_audio_count;
  int                       m_subtitle_count;
  bool                      m_open;
  std::string               m_filename;
  bool                      m_bMatroska;
  bool                      m_bAVI;
  AVFormatContext           *m_pFormatContext;
  AVIOContext               *m_ioContext;
  bool                      m_eof;
  int64_t                   m_chapters[MAX_OMX_CHAPTERS];
  OMXStream                 m_streams[MAX_STREAMS];
  int                       m_chapter_count;
  int64_t                   m_iCurrentPts;
  int                       m_speed;
  unsigned int              m_program;
  pthread_mutex_t           m_lock;
  double                    m_aspect;
  int                       m_width;
  int                       m_height;
  void Lock();
  void UnLock();
  bool SetActiveStreamInternal(OMXStreamType type, unsigned int index);
  bool                      m_seek;
  OMXDvdPlayer              *m_DvdPlayer;

private:
public:
  OMXReader();
  ~OMXReader();
  bool Open(std::string &filename, bool is_url, bool dump_format, bool live, float timeout,
    std::string &cookie, std::string &user_agent, std::string &lavfdopts, std::string &avdict,
    OMXDvdPlayer *dvd);
  void ClearStreams();
  bool Close();
  //void FlushRead();
  bool SeekTime(int64_t time, bool backwords, int64_t *startpts);
  OMXPacket *Read();
  bool GetStreams(bool dump_format = false);
  void AddStream(int id);
  bool IsActive(int stream_index);
  bool IsActive(OMXStreamType type, int stream_index);
  double SelectAspect(AVStream* st, bool& forced);
  bool GetHints(AVStream *stream, COMXStreamInfo *hints);
  bool GetHints(OMXStreamType type, COMXStreamInfo &hints);
  bool IsEof();
  int  AudioStreamCount() { return m_audio_count; };
  int  VideoStreamCount() { return m_video_count; };
  int  SubtitleStreamCount() { return m_subtitle_count; };
  bool SetActiveStream(OMXStreamType type, unsigned int index);
  int  GetChapterCount() { return m_chapter_count; };
  double GetAspectRatio() { return m_aspect; };
  int GetWidth() { return m_width; };
  int GetHeight() { return m_height; };
  OMXPacket *AllocPacket();
  void SetSpeed(int iSpeed);
  void UpdateCurrentPTS();
  int64_t ConvertTimestamp(int64_t pts, int den, int num);
  int GetChapter();
  bool SeekChapter(int chapter, int64_t* startpts);
  int GetAudioIndex() { return (m_audio_index >= 0) ? m_streams[m_audio_index].index : -1; };
  int GetSubtitleIndex() { return (m_subtitle_index >= 0) ? m_streams[m_subtitle_index].index : -1; };
  int GetVideoIndex() { return (m_video_index >= 0) ? m_streams[m_video_index].index : -1; };
  std::string getFilename() const { return m_filename; }

  int GetRelativeIndex(size_t index)
  {
    assert(index < MAX_STREAMS);
    return m_streams[index].index;
  }

  int GetStreamLengthSeconds();
  int64_t GetStreamLengthMicro();
  static double NormalizeFrameduration(double frameduration);
  std::string GetCodecName(OMXStreamType type);
  std::string GetCodecName(OMXStreamType type, unsigned int index);
  std::string GetStreamCodecName(AVStream *stream);
  std::string GetStreamLanguage(OMXStreamType type, unsigned int index);
  int GetStreamByLanguage(OMXStreamType type, const char *lang);
  std::string GetStreamName(OMXStreamType type, unsigned int index);
  std::string GetStreamType(OMXStreamType type, unsigned int index);
  bool CanSeek();
  bool FindDVDSubs(Dimension &d, float &aspect, uint32_t **palette);
};
#endif
