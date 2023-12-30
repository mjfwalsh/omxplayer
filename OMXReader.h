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
#include "OMXClock.h"
#include "utils/simple_geometry.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <unordered_map>


#define MAX_VIDEO_STREAMS 1

class OMXPacket
{
public:
  OMXPacket();
  ~OMXPacket();
  
  AVPacket *avpkt;
  COMXStreamInfo hints;
  enum AVMediaType codec_type;
  int stream_type_index;
};

enum OMXStreamType
{
  OMXSTREAM_NONE      = -1,
  OMXSTREAM_VIDEO     = 0,
  OMXSTREAM_AUDIO     = 1,
  OMXSTREAM_SUBTITLE  = 2,
  OMXSTREAM_END       = 3,
};

enum SeekResult {
  SEEK_SUCCESS = -1,
  SEEK_FAIL = -2,
  SEEK_OUT_OF_BOUNDS = -3,
  SEEK_NO_CHAPTERS = -4,
};

class OMXReader
{
public:
  OMXReader();
  virtual ~OMXReader();

  virtual enum SeekResult SeekTime(int64_t time, bool backwards) = 0;
  virtual enum SeekResult SeekTimeDelta(int delta_secs, int64_t &cur_pts) = 0;
  virtual OMXPacket *Read();
  COMXStreamInfo GetHints(OMXStreamType type, int index);
  bool IsEof();
  inline int  AudioStreamCount() { return m_streams[OMXSTREAM_AUDIO].size(); };
  inline int  VideoStreamCount() { return m_streams[OMXSTREAM_VIDEO].size(); };
  inline int  SubtitleStreamCount() { return m_streams[OMXSTREAM_SUBTITLE].size(); };
  inline double GetAspectRatio() { return m_aspect; };
  inline int GetWidth() { return m_width; };
  inline int GetHeight() { return m_height; };
  void SetSpeed(float iSpeed);
  virtual SeekResult SeekChapter(int delta, int &result_chapter, int64_t &cur_pts) = 0;
  int GetStreamLengthSeconds();
  int64_t GetStreamLengthMicro();
  static double NormalizeFrameduration(double frameduration);
  std::string GetCodecName(OMXStreamType type, unsigned int index);
  void GetMetaData(OMXStreamType type, std::vector<std::string> &list);
  std::string GetStreamLanguage(OMXStreamType type, unsigned int index);
  int GetStreamByLanguage(OMXStreamType type, const char *lang);
  virtual bool CanSeek() = 0;
  bool FindDVDSubs(Dimension &d, float &aspect, uint32_t **palette, uint32_t *buf);
  static void SetCookie(const char *c);
  inline static void SetUserAgent(const char *ua) { s_user_agent.assign(ua); }
  inline static void SetLavDopts(const char *lo)  { s_lavfdopts.assign(lo); }
  static bool SetAvDict(const char *ad);
  static void SetDefaultTimeout(float timeout);
  void info_dump(const std::string &filename);

protected:
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

  bool                      m_bMatroska       = false;
  bool                      m_bAVI            = false;
  AVFormatContext           *m_pFormatContext = NULL;
  bool                      m_eof             = false;
  std::vector<OMXStream>    m_streams[OMXSTREAM_END];
  float                     m_speed           = DVD_PLAYSPEED_NORMAL;
  double                    m_aspect          = 0.0f;
  int                       m_width           = 0;
  int                       m_height          = 0;
  int                       m_dvd_subs        = -1;
  bool                      m_dvd_subs_need_init = false;
  std::unordered_map<int, int>   m_steam_map;
  static std::string        s_cookie;
  static std::string        s_user_agent;
  static std::string        s_lavfdopts;
  static AVDictionary       *s_avdict;
  static int64_t timeout_start;
  static int64_t timeout_default_duration;
  static int64_t timeout_duration;

  std::string GetStreamCodecName(AVStream *stream);
  virtual void GetStreams() = 0;
  int AddStream(int id, const char* lang = NULL);
  double SelectAspect(AVStream* st, bool& forced);
  int64_t ConvertTimestamp(int64_t pts, int den, int num);
  static int interrupt_cb(void *unused = NULL);
  static void reset_timeout(int x);
  bool SetHints(AVStream *stream, COMXStreamInfo *hints);
  virtual uint32_t *getPalette(OMXStream *st, uint32_t *palette) = 0;
};

#endif
