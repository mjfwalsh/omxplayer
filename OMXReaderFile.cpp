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

#include "OMXReader.h"
#include "OMXReaderFile.h"
#include "utils/misc.h"
#include "utils/log.h"

#include <string>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

using namespace std;

OMXReaderFile::OMXReaderFile(string &filename, bool live)
{
  AVDictionary *d = NULL;

  if(s_avdict)
    av_dict_copy(&d, s_avdict, 0);

  if(IsURL(filename))
  {
    if(filename.substr(0, 8) == "shout://" )
      filename.replace(0, 8, "http://");

    // ffmpeg dislikes the useragent from AirPlay urls
    size_t idx = filename.find("|");
    if(idx != string::npos)
      filename = filename.substr(0, idx);

    // Enable seeking if http, ftp
    if(!live && (filename.substr(0,7) == "http://" || filename.substr(0,8) == "https://" ||
        filename.substr(0,6) == "ftp://" || filename.substr(0,7) == "sftp://"))
    {
       av_dict_set(&d, "seekable", "1", 0);
    }

    // set user-agent and cookie
    if(!s_cookie.empty())
       av_dict_set(&d, "cookies", s_cookie.c_str(), 0);

    if(!s_user_agent.empty())
       av_dict_set(&d, "user_agent", s_user_agent.c_str(), 0);
  }

  CLogLog(LOGDEBUG, "COMXPlayer::OpenFile - avformat_open_input %s", filename.c_str());

  int result = avformat_open_input(&m_pFormatContext, filename.c_str(), NULL, &d);
  av_dict_free(&d);
  if(result < 0)
    throw "avformat_open_input failed";

  if(live)
  {
     CLogLog(LOGDEBUG, "COMXPlayer::OpenFile - avformat_open_input disabled SEEKING");
     m_pFormatContext->pb->seekable = 0;
  }

  m_bMatroska = strncmp(m_pFormatContext->iformat->name, "matroska", 8) == 0; // for "matroska.webm"
  m_bAVI = strcmp(m_pFormatContext->iformat->name, "avi") == 0;

  // analyse very short to speed up mjpeg playback start
  if(strcmp(m_pFormatContext->iformat->name, "mjpeg") == 0)
    m_pFormatContext->max_analyze_duration = 500000;

  if(m_bMatroska)
    m_pFormatContext->max_analyze_duration = 0;

  if (live)
    m_pFormatContext->flags |= AVFMT_FLAG_NOBUFFER;

  if(avformat_find_stream_info(m_pFormatContext, NULL) < 0)
    throw "avformat_find_stream_info failed";

  // fill in rest of metadata
  GetStreams();
  GetChapters();
}

enum SeekResult OMXReaderFile::SeekTimeDelta(int delta, int64_t &cur_pts)
{
  int64_t seek_pts = cur_pts + (int64_t)delta * AV_TIME_BASE;
  enum SeekResult r = SeekTime(seek_pts, delta < 0);

  if(r == SEEK_SUCCESS)
    cur_pts = seek_pts;

  return r;
}

enum SeekResult OMXReaderFile::SeekTime(int64_t seek_pts, bool backwards)
{
  if(!CanSeek())
    return SEEK_FAIL;

  int flags = backwards ? AVSEEK_FLAG_BACKWARD : 0;
  int64_t seek_value = seek_pts;
  if (m_pFormatContext->start_time != (int64_t)AV_NOPTS_VALUE)
    seek_value += m_pFormatContext->start_time;

  reset_timeout(1);
  bool success = av_seek_frame(m_pFormatContext, -1, seek_value, flags) >= 0;

  // demuxer will return failure, if you seek to eof
  m_eof = !success;

  return success ? SEEK_SUCCESS : SEEK_OUT_OF_BOUNDS;
}

bool OMXReaderFile::CanSeek()
{
  return m_pFormatContext->pb->seekable & AVIO_SEEKABLE_NORMAL;
}

void OMXReaderFile::GetStreams()
{
  for (unsigned int i = 0; i < m_pFormatContext->nb_streams; i++)
    AddStream(i);
}

SeekResult OMXReaderFile::SeekChapter(int delta, int &result_chapter, int64_t &cur_pts)
{
  if(cur_pts == AV_NOPTS_VALUE) return SEEK_FAIL;

  // We have no chapters to seek to
  if(m_chapter_count == 0)
    return SEEK_NO_CHAPTERS;

  // Find current chapter
  int current_chapter = 0;
  for(; current_chapter < m_chapter_count - 1; current_chapter++)
    if(cur_pts >=   m_chapters[current_chapter] && cur_pts <  m_chapters[current_chapter+1])
      break;

  // turn delta into absolute value and check in within range
  int new_chapter = current_chapter + delta;
  if(new_chapter < 0 || new_chapter >= m_chapter_count)
    return SEEK_OUT_OF_BOUNDS;

  SeekResult r = SeekTime(m_chapters[new_chapter], delta < 0);
  if(r == SEEK_SUCCESS)
  {
    // update time
    cur_pts = m_chapters[new_chapter];

    // convert delta to new chapter
    result_chapter = new_chapter;
  }

  return r;
}

void OMXReaderFile::GetChapters()
{
  m_chapter_count = (m_pFormatContext->nb_chapters > MAX_OMX_CHAPTERS) ? MAX_OMX_CHAPTERS : m_pFormatContext->nb_chapters;
  for(int i = 0; i < m_chapter_count; i++)
  {
    AVChapter *chapter = m_pFormatContext->chapters[i];
    if(!chapter)
    {
      m_chapter_count = i;
      break;
    }

    m_chapters[i] = ConvertTimestamp(chapter->start, chapter->time_base.den, chapter->time_base.num);
  }
}

uint32_t *OMXReaderFile::getPalette(OMXStream *st, uint32_t *palette)
{
  const char *p = (const char*)st->extradata;

  const char *end = p + st->extrasize;
  while(p < end) {
    if(strncmp(p, "palette:", 8) == 0) {
      p += 8;
      goto found_palette;
    }
    while(p < end && *p++ != '\n');
  }
  return NULL;

  found_palette:

  char *next;
  for(int i = 0; i < 16; i++) {
    palette[i] = strtoul(p, &next, 16);
    if(p == next) {
      return NULL;
    } else {
      p = next;
    }

    while(*p == ',' || *p == ' ') p++;
  }

  return palette;
}
