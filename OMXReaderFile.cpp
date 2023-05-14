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

OMXReaderFile::OMXReaderFile(string &filename, bool dump_format, bool live)
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

  AVInputFormat *iformat = NULL;
  int result = avformat_open_input(&m_pFormatContext, filename.c_str(), iformat, &d);
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
  if (iformat && (strcmp(iformat->name, "mjpeg") == 0) && m_ioContext->seekable == 0)
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

  // print chapter info
  if(dump_format)
    info_dump(filename);
}


enum SeekResult OMXReaderFile::SeekTime(int64_t seek_pts, int64_t *cur_pts, bool backwards)
{
  if(!CanSeek())
    return SEEK_FAIL;

  if(m_ioContext)
    m_ioContext->buf_ptr = m_ioContext->buf_end;

  if(cur_pts)
    backwards = seek_pts < *cur_pts;

  int flags = backwards ? AVSEEK_FLAG_BACKWARD : 0;
  int64_t seek_value = seek_pts;
  if (m_pFormatContext->start_time != (int64_t)AV_NOPTS_VALUE)
    seek_value += m_pFormatContext->start_time;

  reset_timeout(1);
  bool success = av_seek_frame(m_pFormatContext, -1, seek_value, flags) >= 0;

  if(success && cur_pts != NULL)
    *cur_pts = seek_pts;

  // demuxer will return failure, if you seek to eof
  m_eof = !success;

  return success ? SEEK_SUCCESS : SEEK_OUT_OF_BOUNDS;
}


void OMXReaderFile::GetStreams()
{
  unsigned int program = UINT_MAX;

  if (m_pFormatContext->nb_programs)
  {
    // look for first non empty stream and discard nonselected programs
    for (unsigned int i = 0; i < m_pFormatContext->nb_programs; i++)
    {
      if(program == UINT_MAX && m_pFormatContext->programs[i]->nb_stream_indexes > 0)
        program = i;
      else
        m_pFormatContext->programs[i]->discard = AVDISCARD_ALL;
    }
  }

  if(program != UINT_MAX)
  {
    // add streams from selected program
    for (unsigned int i = 0; i < m_pFormatContext->programs[program]->nb_stream_indexes; i++)
      AddStream(m_pFormatContext->programs[program]->stream_index[i]);
  }
  else
  {
    // if there were no programs or they were all empty, add all streams
    for (unsigned int i = 0; i < m_pFormatContext->nb_streams; i++)
      AddStream(i);
  }
}

SeekResult OMXReaderFile::SeekChapter(int &chapter, int64_t &cur_pts)
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
  int new_chapter = current_chapter + chapter;
  if(new_chapter < 0 || new_chapter >= m_chapter_count)
    return SEEK_OUT_OF_BOUNDS;

  // convert delta new chapter
  chapter = new_chapter;

  return SeekTime(m_chapters[new_chapter], &cur_pts);
}

