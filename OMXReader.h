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

extern "C" {
#include <libavformat/avformat.h>
}

#include "OMXStreamInfo.h"
#include "utils/simple_geometry.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <unordered_map>

class OMXDvdPlayer;


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
  explicit OMXPacket(AVFormatContext *format_context);
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

  int                       m_video_count     = 0;
  int                       m_audio_count     = 0;
  int                       m_subtitle_count  = 0;
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
  float                     m_speed;
  double                    m_aspect          = 0.0f;
  int                       m_width           = 0;
  int                       m_height          = 0;
  OMXDvdPlayer              *m_DvdPlayer      = NULL;
  std::unordered_map<int, int>   m_steam_map;
  static std::string        s_cookie;
  static std::string        s_user_agent;
  static std::string        s_lavfdopts;
  static std::string        s_avdict;

public:
  // results for chapter seek function
  enum SeekResult {
    SEEK_SUCCESS,
    SEEK_ERROR,
    SEEK_OUT_OF_BOUNDS,
    SEEK_NO_CHAPTERS
  };

  OMXReader(std::string &filename, bool dump_format, bool live, OMXDvdPlayer *dvd);
  ~OMXReader();

  bool SeekTime(int64_t time, int64_t *cur_pts, bool backwards = false);
  bool SeekBytes(int64_t seek_bytes, bool backwords);
  OMXPacket *Read();
  bool SetHints(AVStream *stream, COMXStreamInfo *hints);
  COMXStreamInfo GetHints(OMXStreamType type, int index);
  bool IsEof();
  int  AudioStreamCount() { return m_audio_count; };
  int  VideoStreamCount() { return m_video_count; };
  int  SubtitleStreamCount() { return m_subtitle_count; };
  double GetAspectRatio() { return m_aspect; };
  int GetWidth() { return m_width; };
  int GetHeight() { return m_height; };
  void SetSpeed(float iSpeed);
  SeekResult SeekChapter(int &chapter, int64_t &cur_pts);
  int GetStreamLengthSeconds();
  int64_t GetStreamLengthMicro();
  static double NormalizeFrameduration(double frameduration);
  std::string GetCodecName(OMXStreamType type, unsigned int index);
  void GetMetaData(OMXStreamType type, std::vector<std::string> &list);
  std::string GetStreamLanguage(OMXStreamType type, unsigned int index);
  int GetStreamByLanguage(OMXStreamType type, const char *lang);
  bool CanSeek();
  bool FindDVDSubs(Dimension &d, float &aspect, uint32_t **palette);
  static void SetCookie(const char *c);
  static void SetUserAgent(const char *ua) { s_user_agent.assign(ua); }
  static void SetLavDopts(const char *lo)  { s_lavfdopts.assign(lo); }
  static void SetAvDict(const char *ad)    { s_avdict.assign(ad); }
  static void SetDefaultTimeout(float timeout);

private:
  std::string GetStreamCodecName(AVStream *stream);
  void GetStreams();
  void GetDvdStreams();
  void GetChapters();
  void AddStream(int id);
  double SelectAspect(AVStream* st, bool& forced);
  int64_t ConvertTimestamp(int64_t pts, int den, int num);
  void AddMissingSubtitleStream(int id);
  void info_dump(const std::string &filename);
};

#endif
