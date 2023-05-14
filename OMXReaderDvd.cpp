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
#include "OMXReaderDvd.h"
#include "OMXDvdPlayer.h"
#include "utils/log.h"

#include <string>
#include <stdexcept>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

using namespace std;

#ifndef FFMPEG_FILE_BUFFER_SIZE
#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg
#endif

OMXReaderDvd::OMXReaderDvd(string &filename, bool dump_format, OMXDvdPlayer *dvd)
{
  m_DvdPlayer   = dvd;

  CLogLog(LOGDEBUG, "COMXPlayer::OpenFile - open dvd %s ", filename.c_str());

  unsigned char *buffer = (unsigned char*)av_malloc(FFMPEG_FILE_BUFFER_SIZE);
  if(!buffer)
    throw "av_malloc failed";

  m_ioContext = avio_alloc_context(buffer, FFMPEG_FILE_BUFFER_SIZE, 0, m_DvdPlayer, dvd_read, NULL, dvd_seek);
  if(!m_ioContext)
    throw "avio_alloc_context failed";

  AVInputFormat *iformat = NULL;
  av_probe_input_buffer(m_ioContext, &iformat, NULL, NULL, 0, 0);
  if(!iformat)
    throw "av_probe_input_buffer failed";

  m_pFormatContext->pb = m_ioContext;
  int result = avformat_open_input(&m_pFormatContext, NULL, iformat, &s_avdict);
  av_dict_free(&s_avdict);
  if(result < 0)
    throw "avformat_open_input failed";

  if(avformat_find_stream_info(m_pFormatContext, NULL) < 0)
    throw "avformat_find_stream_info failed";

  m_pFormatContext->duration = (int64_t)m_DvdPlayer->getCurrentTrackLength() * 1000;

  GetStreams();

  // print chapter info
  if(dump_format)
  {
    info_dump(filename);
    m_DvdPlayer->info_dump();
  }
}


enum SeekResult OMXReaderDvd::SeekTime(int64_t seek_pts, int64_t *cur_pts, bool backwards)
{
  if(!CanSeek())
    return SEEK_FAIL;

  if(m_ioContext)
    m_ioContext->buf_ptr = m_ioContext->buf_end;

  if(cur_pts)
    backwards = seek_pts < *cur_pts;

  int flags = AVSEEK_FLAG_BYTE | (backwards ? AVSEEK_FLAG_BACKWARD : 0);
  int64_t seek_value;

  int cur_ch = cur_pts ? m_DvdPlayer->getChapter(*cur_pts) : -1;
  int seek_ch = m_DvdPlayer->GetChapterInfo(seek_pts, seek_value);
  if(seek_ch == -1)
  {
    m_eof = true;
    return SEEK_OUT_OF_BOUNDS;
  }
  else if(cur_ch == seek_ch)
    return SEEK_FAIL;

  reset_timeout(1);
  bool success = av_seek_frame(m_pFormatContext, -1, seek_value, flags) >= 0;

  if(success && cur_pts != NULL)
    *cur_pts = seek_pts;

  // demuxer will return failure, if you seek to eof
  m_eof = !success;

  return success ? SEEK_SUCCESS : SEEK_OUT_OF_BOUNDS;
}

void OMXReaderDvd::GetStreams()
{
  std::unordered_map<int, int> stream_lookup;
  for(unsigned int i = 0; i < m_pFormatContext->nb_streams; i++)
    stream_lookup[m_pFormatContext->streams[i]->id] = i;

  auto AddStreamById = [&](int id, const char *lang = NULL)
  {
    try {
      AddStream(stream_lookup.at(id), lang);
      return true;
    }
    catch(std::out_of_range const&) {
      return false;
    }
  };

  // assume video is always 0x1e0
  AddStreamById(0x1e0);

  // audio streams
  for(int i = 0; i < m_DvdPlayer->GetAudioStreamCount(); i++)
    AddStreamById(m_DvdPlayer->GetAudioStreamId(i), m_DvdPlayer->GetAudioStreamLanguage(i));

  // subtitle streams
  for(int i = 0; i < m_DvdPlayer->GetSubtitleStreamCount(); i++)
  {
    int id = m_DvdPlayer->GetSubtitleStreamId(i);
    const char *lang = m_DvdPlayer->GetAudioStreamLanguage(i);
    
    if(!AddStreamById(id, lang))
    {
      AddMissingSubtitleStream(id);
      AddStream(m_pFormatContext->nb_streams - 1, lang);
    }
  }
}


SeekResult OMXReaderDvd::SeekChapter(int &chapter, int64_t &cur_pts)
{
  if(cur_pts == AV_NOPTS_VALUE) return SEEK_FAIL;

  bool backwards = chapter < 0;
  int cur_ch = m_DvdPlayer->getChapter(cur_pts);
  if(cur_ch == -1)
    return SEEK_OUT_OF_BOUNDS;

  int seek_ch = cur_ch + chapter;

  // check if within bounds
  if(seek_ch < 0 || seek_ch >= m_DvdPlayer->TotalChapters())
    return SEEK_OUT_OF_BOUNDS;

  // Do the seek
  if(!SeekBytes(m_DvdPlayer->GetChapterBytePos(seek_ch), backwards))
    return SEEK_FAIL;

  // convert delta new chapter
  chapter = seek_ch;

  return SEEK_SUCCESS;
}


void OMXReaderDvd::AddMissingSubtitleStream(int id)
{
  // We've found a new subtitle stream
  AVStream *st = avformat_new_stream(m_pFormatContext, NULL);
  if (!st)
    throw "This isn't meant to happen";
  
  st->id = id;
  st->codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;
  st->codecpar->codec_id = AV_CODEC_ID_DVD_SUBTITLE;
}


int OMXReaderDvd::dvd_read(void *h, uint8_t* buf, int size)
{
  reset_timeout(1);
  if(interrupt_cb())
    return -1;

  OMXDvdPlayer *reader = static_cast<OMXDvdPlayer*>(h);
  int ret = reader->Read(buf, size);

  if (ret == 0) {
    if(reader->IsEOF())
      return AVERROR_EOF;
    else puts("OMXDvdPlayer failed to read anything");
  }

  return ret;
}

int64_t OMXReaderDvd::dvd_seek(void *h, int64_t pos, int whence)
{
  reset_timeout(1);
  if(interrupt_cb())
    return -1;

  OMXDvdPlayer *reader = static_cast<OMXDvdPlayer*>(h);
  return reader->Seek(pos, whence);
}
