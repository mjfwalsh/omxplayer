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
#include <unordered_map>

class OMXDvdPlayer;

using namespace std;


#ifndef FFMPEG_FILE_BUFFER_SIZE
#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg
#endif

#define MAX_VIDEO_STREAMS 3
#define MAX_AUDIO_STREAMS 32
#define MAX_SUBTITLE_STREAMS 50

#define MAX_OMX_CHAPTERS 64

class OMXPacket : public AVPacket
{
  public: 
  OMXPacket();
  ~OMXPacket();
  
  COMXStreamInfo hints;
  enum AVMediaType codec_type;
  int index;
};

enum OMXStreamType
{
  OMXSTREAM_NONE      = 0,
  OMXSTREAM_AUDIO     = 1,
  OMXSTREAM_VIDEO     = 2,
  OMXSTREAM_SUBTITLE  = 3
};

class OMXReader
{
private:
  class OMXStream
  {
  public:
    std::string    language;
    std::string    name;
    std::string    codec_name;
    AVStream       *stream     = NULL;
    OMXStreamType  type      = OMXSTREAM_NONE;
    int            id          = 0;
    void           *extradata  = NULL;
    unsigned int   extrasize  = 0;
    COMXStreamInfo hints;
  };

  int                       m_video_index     = -1;
  int                       m_audio_index     = -1;
  int                       m_subtitle_index  = -1;
  int                       m_video_count     = 0;
  int                       m_audio_count     = 0;
  int                       m_subtitle_count  = 0;
  bool                      m_open            = false;
  std::string               m_filename;
  bool                      m_bMatroska       = false;
  bool                      m_bAVI            = false;
  AVFormatContext           *m_pFormatContext = NULL;
  AVIOContext               *m_ioContext      = NULL;
  bool                      m_eof             = false;
  int64_t                   m_chapters[MAX_OMX_CHAPTERS];
  OMXStream                 m_audio_streams[MAX_AUDIO_STREAMS];
  OMXStream                 m_video_streams[MAX_VIDEO_STREAMS];
  OMXStream                 m_subtitle_streams[MAX_SUBTITLE_STREAMS];
  int                       m_chapter_count   = 0;
  int                       m_speed;
  pthread_mutex_t           m_lock;
  double                    m_aspect          = 0.0f;
  int                       m_width           = 0;
  int                       m_height          = 0;
  void Lock();
  void UnLock();
  OMXDvdPlayer              *m_DvdPlayer      = NULL;
  unordered_map<int, int>   m_steam_map;

public:
  // results for chapter seek function
  enum SeekResult {
    SEEK_SUCCESS,
    SEEK_ERROR,
    SEEK_OUT_OF_BOUNDS,
    SEEK_NO_CHAPTERS
  };

  OMXReader();
  ~OMXReader();
  bool Open(std::string &filename, bool is_url, bool dump_format, bool live, float timeout,
    std::string &cookie, std::string &user_agent, std::string &lavfdopts, std::string &avdict,
    OMXDvdPlayer *dvd);
  bool Close();
  bool SeekTime(int64_t time, bool backwords, int64_t *startpts);
  OMXPacket *Read();
  bool GetHints(AVStream *stream, COMXStreamInfo *hints);
  void GetHints(OMXStreamType type, int index, COMXStreamInfo &hints);
  bool IsEof();
  int  AudioStreamCount() { return m_audio_count; };
  int  VideoStreamCount() { return m_video_count; };
  int  SubtitleStreamCount() { return m_subtitle_count; };
  double GetAspectRatio() { return m_aspect; };
  int GetWidth() { return m_width; };
  int GetHeight() { return m_height; };
  void SetSpeed(int iSpeed);
  SeekResult SeekChapter(int *chapter, int64_t cur_pts, int64_t* new_pts);
  std::string getFilename() const { return m_filename; }
  int GetStreamLengthSeconds();
  int64_t GetStreamLengthMicro();
  static double NormalizeFrameduration(double frameduration);
  std::string GetCodecName(OMXStreamType type, unsigned int index);
  std::string GetStreamMetaData(OMXStreamType type, unsigned int index);
  std::string GetStreamLanguage(OMXStreamType type, unsigned int index);
  int GetStreamByLanguage(OMXStreamType type, const char *lang);
  bool CanSeek();
  bool FindDVDSubs(Dimension &d, float &aspect, uint32_t **palette);
private:
  std::string GetStreamCodecName(AVStream *stream);
  void GetStreams();
  void GetDvdStreams();
  void GetChapters();
  void ClearStreams();
  void AddStream(int id);
  double SelectAspect(AVStream* st, bool& forced);
  int64_t ConvertTimestamp(int64_t pts, int den, int num);
  void AddMissingSubtitleStream(int id);
};
#endif
