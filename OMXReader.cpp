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
#include "OMXClock.h"
#include "OMXDvdPlayer.h"
#include "utils/log.h"

#include <stdio.h>
#include <unordered_map>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

#define MAX_DATA_SIZE_VIDEO    8 * 1024 * 1024
#define MAX_DATA_SIZE_AUDIO    2 * 1024 * 1024
#define MAX_DATA_SIZE          10 * 1024 * 1024

using namespace std;

std::string OMXReader::s_cookie;
std::string OMXReader::s_user_agent;
std::string OMXReader::s_lavfdopts;
std::string OMXReader::s_avdict;

static int64_t timeout_start;
static int64_t timeout_default_duration = (int64_t)1e10; // amount of time file/network operation can stall for before timing out
static int64_t timeout_duration;

#define RESET_TIMEOUT(x) do { \
  timeout_start = OMXClock::CurrentHostCounter(); \
  timeout_duration = (x) * timeout_default_duration; \
} while (0)

OMXPacket::OMXPacket(AVFormatContext *format_context)
{
  dts  = AV_NOPTS_VALUE;
  pts  = AV_NOPTS_VALUE;
  duration = AV_NOPTS_VALUE;
  size = 0;
  data = NULL;
  stream_index = -1;
  codec_type = AVMEDIA_TYPE_UNKNOWN;

  if(av_read_frame(format_context, this) < 0)
    throw "No packet";
}


OMXPacket::~OMXPacket()
{
  	av_packet_unref(this);
}

static int interrupt_cb(void *unused)
{
  int ret = 0;
  if (timeout_duration && OMXClock::CurrentHostCounter() - timeout_start > timeout_duration)
  {
    CLogLog(LOGERROR, "COMXPlayer::interrupt_cb - Timed out");
    ret = 1;
  }
  return ret;
}

static int dvd_read(void *h, uint8_t* buf, int size)
{
  RESET_TIMEOUT(1);
  if(interrupt_cb(NULL))
    return -1;

  OMXDvdPlayer *reader =(OMXDvdPlayer*) h;
  int ret = reader->Read(buf, size);

  if (ret == 0) {
    if(reader->IsEOF())
      return AVERROR_EOF;
    else puts("OMXDvdPlayer failed to read anything");
  }

  return ret;
}

static int64_t dvd_seek(void *h, int64_t pos, int whence)
{
  RESET_TIMEOUT(1);
  if(interrupt_cb(NULL))
    return -1;

  OMXDvdPlayer *reader =(OMXDvdPlayer*) h;
  if(whence == AVSEEK_SIZE)
    return reader->GetSizeInBytes();
  else
    return reader->Seek(pos, whence);
}

void OMXReader::SetDefaultTimeout(float timeout)
{
  timeout_default_duration = (int64_t) (timeout * 1e9);
}

OMXReader::OMXReader(
	std::string &filename,
	bool is_url,
	bool dump_format,
	bool live,
	OMXDvdPlayer *dvd)
{
  m_speed       = DVD_PLAYSPEED_NORMAL;
  m_DvdPlayer   = dvd;
  const AVIOInterruptCB int_cb = { interrupt_cb, NULL };
  RESET_TIMEOUT(3);

  avformat_network_init();

  if(dump_format && av_log_get_level() < AV_LOG_INFO)
    av_log_set_level(AV_LOG_INFO);

  int           result    = -1;
  AVInputFormat *iformat  = NULL;
  unsigned char *buffer   = NULL;

  m_pFormatContext     = avformat_alloc_context();

  result = av_set_options_string(m_pFormatContext, s_lavfdopts.c_str(), ":", ",");

  if (result < 0)
  {
    CLogLog(LOGERROR, "COMXPlayer::OpenFile - invalid lavfdopts %s ", s_lavfdopts.c_str());
    throw "Invalid lavfdopts";
  }

  AVDictionary *d = NULL;
  result = av_dict_parse_string(&d, s_avdict.c_str(), ":", ",", 0);

  if (result < 0)
  {
    CLogLog(LOGERROR, "COMXPlayer::OpenFile - invalid avdict %s ", s_avdict.c_str());
    throw "Invalid avdict";
  }

  // set the interrupt callback, appeared in libavformat 53.15.0
  m_pFormatContext->interrupt_callback = int_cb;

  // if format can be nonblocking, let's use that
  m_pFormatContext->flags |= AVFMT_FLAG_NONBLOCK;

  if(m_DvdPlayer)
  {
    CLogLog(LOGDEBUG, "COMXPlayer::OpenFile - open dvd %s ", filename.c_str());

    buffer = (unsigned char*)av_malloc(FFMPEG_FILE_BUFFER_SIZE);
    m_ioContext = avio_alloc_context(buffer, FFMPEG_FILE_BUFFER_SIZE, 0, m_DvdPlayer, dvd_read, NULL, dvd_seek);

    av_probe_input_buffer(m_ioContext, &iformat, NULL, NULL, 0, 0);

    if(!iformat)
    {
      CLogLog(LOGERROR, "COMXPlayer::OpenFile - av_probe_input_buffer %s ", filename.c_str());
      throw "av_probe_input_buffer failed";
    }

    m_pFormatContext->pb = m_ioContext;
    result = avformat_open_input(&m_pFormatContext, NULL, iformat, &d);
    if(result < 0)
    {
      throw "avformat_open_input failed";
    }
  }
  else
  {
    if(is_url)
    {
      if(filename.substr(0, 8) == "shout://" )
        filename.replace(0, 8, "http://");

      // ffmpeg dislikes the useragent from AirPlay urls
      //int idx = filename.Find("|User-Agent=AppleCoreMedia");
      size_t idx = filename.find("|");
      if(idx != string::npos)
        filename = filename.substr(0, idx);

      // Enable seeking if http, ftp
      if(filename.substr(0,7) == "http://" || filename.substr(0,6) == "ftp://" ||
         filename.substr(0,7) == "sftp://")
      {
         if(!live)
         {
            av_dict_set(&d, "seekable", "1", 0);
         }
         if(!s_cookie.empty())
         {
            av_dict_set(&d, "cookies", s_cookie.c_str(), 0);
         }
         if(!s_user_agent.empty())
         {
            av_dict_set(&d, "user_agent", s_user_agent.c_str(), 0);
         }
      }
    }

    CLogLog(LOGDEBUG, "COMXPlayer::OpenFile - avformat_open_input %s", filename.c_str());
    result = avformat_open_input(&m_pFormatContext, filename.c_str(), iformat, &d);
    av_dict_free(&d);
    if(result < 0)
    {
      CLogLog(LOGERROR, "COMXPlayer::OpenFile - avformat_open_input %s", filename.c_str());
      throw "avformat_open_input failed";
    }

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
  }

  result = avformat_find_stream_info(m_pFormatContext, NULL);
  if(result < 0)
  {
    throw "avformat_find_stream_info failed";
  }

  // get stream info
  if(m_DvdPlayer)
    GetDvdStreams();
  else
    GetStreams();

  // get chapter info
  GetChapters();

  // print chapter info
  if(dump_format)
  {
    for(int i = 0; i < m_chapter_count; i++)
      printf("Chapter : \t%d \t%8.2f\n", i, (double)m_chapters[i]/AV_TIME_BASE);

    av_dump_format(m_pFormatContext, 0, filename.c_str(), 0);
  }
}


OMXReader::~OMXReader()
{
  if (m_pFormatContext)
  {
    if (m_ioContext && m_pFormatContext->pb && m_pFormatContext->pb != m_ioContext)
    {
      CLogLog(LOGWARNING, "CDVDDemuxFFmpeg::Dispose - demuxer changed our byte context behind our back, possible memleak");
      m_ioContext = m_pFormatContext->pb;
    }
    avformat_close_input(&m_pFormatContext);
  }

  if(m_ioContext)
  {
    av_free(m_ioContext->buffer);
    avio_context_free(&m_ioContext);
  }

  avformat_network_deinit();
}

bool OMXReader::SeekTime(int64_t time, bool backwords, int64_t *startpts)
{
  if(time < 0)
    time = 0;

  if(!m_pFormatContext)
    return false;

  if(m_ioContext)
    m_ioContext->buf_ptr = m_ioContext->buf_end;

  int64_t seek_pts = time;
  if (m_pFormatContext->start_time != (int64_t)AV_NOPTS_VALUE)
    seek_pts += m_pFormatContext->start_time;

  RESET_TIMEOUT(1);
  int ret = av_seek_frame(m_pFormatContext, -1, seek_pts, backwords ? AVSEEK_FLAG_BACKWARD : 0);

  // in this case the start time is requested time
  if(startpts)
    *startpts = time;

  // demuxer will return failure, if you seek to eof
  m_eof = false;
  if (ret < 0)
  {
    m_eof = true;
    ret = 0;
  }

  return (ret >= 0);
}

OMXPacket *OMXReader::Read()
{
  if(!m_pFormatContext || m_eof)
    return NULL;

  // assume we are not eof
  if(m_pFormatContext->pb)
    m_pFormatContext->pb->eof_reached = 0;

  // timeout
  RESET_TIMEOUT(1);

  // create packet
  OMXPacket *m_omx_pkt;
  try
  {
    m_omx_pkt = new OMXPacket(m_pFormatContext);
    m_omx_pkt->index = m_steam_map.at(m_omx_pkt->stream_index);
  }
  catch(const char *msg)
  {
    m_eof = true;
    return NULL;
  }
  catch(std::out_of_range const&)
  {
    delete m_omx_pkt;
    return NULL;
  }

  if (m_omx_pkt->size < 0 || interrupt_cb(NULL))
  {
    // XXX, in some cases ffmpeg returns a negative packet size
    if(m_pFormatContext->pb && !m_pFormatContext->pb->eof_reached)
    {
      CLogLog(LOGERROR, "OMXReader::Read no valid packet");
      //FlushRead();
    }

    delete m_omx_pkt;

    m_eof = true;
    return NULL;
  }

  AVStream *pStream = m_pFormatContext->streams[m_omx_pkt->stream_index];
  m_omx_pkt->codec_type = pStream->codecpar->codec_type;

  if(m_bMatroska && m_omx_pkt->codec_type == AVMEDIA_TYPE_VIDEO)
  { // matroska can store different timestamps
    // for different formats, for native stored
    // stuff it is pts, but for ms compatibility
    // tracks, it is really dts. sadly ffmpeg
    // sets these two timestamps equal all the
    // time, so we select it here instead
    if(pStream->codecpar->codec_tag == 0)
      m_omx_pkt->dts = AV_NOPTS_VALUE;
    else
      m_omx_pkt->pts = AV_NOPTS_VALUE;
  }

  if(m_bAVI && m_omx_pkt->codec_type == AVMEDIA_TYPE_VIDEO)
  {
    // AVI's always have borked pts, specially if m_pFormatContext->flags includes
    // AVFMT_FLAG_GENPTS so always use dts
    m_omx_pkt->pts = AV_NOPTS_VALUE;
  }

  GetHints(pStream, &m_omx_pkt->hints);

  // check if stream has passed full duration, needed for live streams
  // Do this before we convert dts and pts values
  if(m_omx_pkt->dts != (int64_t)AV_NOPTS_VALUE)
  {
    int64_t duration = m_omx_pkt->dts;
    if(pStream->start_time != AV_NOPTS_VALUE)
      duration -= pStream->start_time;

    if(duration > pStream->duration)
    {
      pStream->duration = duration;
      duration = av_rescale_rnd(pStream->duration, (int64_t)pStream->time_base.num * AV_TIME_BASE, 
                                            pStream->time_base.den, AV_ROUND_NEAR_INF);
      if (m_pFormatContext->duration == AV_NOPTS_VALUE || duration > m_pFormatContext->duration)
        m_pFormatContext->duration = duration;
    }
  }

  m_omx_pkt->dts = ConvertTimestamp(m_omx_pkt->dts, pStream->time_base.den, pStream->time_base.num);
  m_omx_pkt->pts = ConvertTimestamp(m_omx_pkt->pts, pStream->time_base.den, pStream->time_base.num);
  m_omx_pkt->duration = DVD_SEC_TO_MICROSEC((double)m_omx_pkt->duration * pStream->time_base.num / pStream->time_base.den);

  return m_omx_pkt;
}


void OMXReader::GetDvdStreams()
{
  std::unordered_map<int, int> stream_lookup;
  for(unsigned int i = 0; i < m_pFormatContext->nb_streams; i++)
    stream_lookup[m_pFormatContext->streams[i]->id] = i;

  auto AddStreamById = [&](int id)
  {
    try {
      AddStream(stream_lookup.at(id));
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
    AddStreamById(m_DvdPlayer->GetAudioStreamId(i));

  // subtitle streams
  for(int i = 0; i < m_DvdPlayer->GetSubtitleStreamCount(); i++)
  {
    int id = m_DvdPlayer->GetSubtitleStreamId(i);
    
    if(!AddStreamById(id))
    {
      AddMissingSubtitleStream(id);
      AddStream(m_pFormatContext->nb_streams - 1);
    }
  }
}

void OMXReader::GetStreams()
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

void OMXReader::GetChapters()
{
  for(int i = 0; i < MAX_OMX_CHAPTERS; i++)
  {
    m_chapters[i] = 0;
  }

  m_chapter_count = 0;

  if(m_DvdPlayer)
  {
    m_chapter_count = (m_DvdPlayer->TotalChapters() > MAX_OMX_CHAPTERS) ? MAX_OMX_CHAPTERS : m_DvdPlayer->TotalChapters();
    for(int i = 0; i < m_chapter_count; i++)
      m_chapters[i]   = m_DvdPlayer->GetChapterStartTime(i);
  }
  else
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
}

void OMXReader::AddStream(int id)
{
  if(!m_pFormatContext)
    return;

  AVStream *pStream = m_pFormatContext->streams[id];

  OMXStream *this_stream;
  switch (pStream->codecpar->codec_type)
  {
    case AVMEDIA_TYPE_AUDIO:
      if(m_audio_count == MAX_AUDIO_STREAMS) return;

      this_stream              = &m_audio_streams[m_audio_count];
      this_stream->type        = OMXSTREAM_AUDIO;
      m_steam_map[id]          = m_audio_count;
      if(m_DvdPlayer)
        this_stream->language = m_DvdPlayer->GetAudioStreamLanguage(m_audio_count);
      
      m_audio_count++;
      break;
    case AVMEDIA_TYPE_VIDEO:
      // discard if it's a picture attachment (e.g. album art embedded in MP3 or AAC)
      if(m_video_count == MAX_VIDEO_STREAMS || (pStream->disposition & AV_DISPOSITION_ATTACHED_PIC))
        return;

      this_stream              = &m_video_streams[m_video_count];
      this_stream->type        = OMXSTREAM_VIDEO;
      m_steam_map[id]          = m_video_count;
      m_video_count++;
      break;
    case AVMEDIA_TYPE_SUBTITLE:
      if(m_subtitle_count == MAX_SUBTITLE_STREAMS) return;

      this_stream              = &m_subtitle_streams[m_subtitle_count];
      this_stream->type        = OMXSTREAM_SUBTITLE;
      m_steam_map[id]          = m_subtitle_count;
      if(m_DvdPlayer)
        this_stream->language = m_DvdPlayer->GetSubtitleStreamLanguage(m_subtitle_count);

      m_subtitle_count++;
      break;
    default:
      return;
  }

  // The rest are the same for all stream types
  this_stream->stream      = pStream;
  this_stream->codec_name  = GetStreamCodecName(pStream);
  this_stream->id          = id;
  GetHints(pStream, &this_stream->hints);

  AVDictionaryEntry *langTag = av_dict_get(pStream->metadata, "language", NULL, 0);
  if (langTag)
    this_stream->language = langTag->value;

  AVDictionaryEntry *titleTag = av_dict_get(pStream->metadata, "title", NULL, 0);
  if (titleTag)
    this_stream->name = titleTag->value;

  if( pStream->codecpar->extradata && pStream->codecpar->extradata_size > 0 )
  {
    this_stream->extrasize = pStream->codecpar->extradata_size;
    this_stream->extradata = pStream->codecpar->extradata;
  }
  else
  {
    this_stream->extrasize = 0;
    this_stream->extradata = NULL;
  }
}
double OMXReader::SelectAspect(AVStream* st, bool& forced)
{
  /* if stream aspect unknown or resolves to 1, use codec aspect */
  /* but don't do this in Matroska containers */
  if(!m_bMatroska && (st->sample_aspect_ratio.num == 0 || st->sample_aspect_ratio.num == st->sample_aspect_ratio.den)
    && st->codecpar->sample_aspect_ratio.num != 0 && st->codecpar->sample_aspect_ratio.den != 0)
  {
    forced = false;
    return av_q2d(st->codecpar->sample_aspect_ratio);
  }

  forced = true;
  if(st->sample_aspect_ratio.num != 0 && st->sample_aspect_ratio.den != 0)
    return av_q2d(st->sample_aspect_ratio);

  return 1.0;
}

bool OMXReader::GetHints(AVStream *stream, COMXStreamInfo *hints)
{
  if(!hints || !stream)
    return false;

  hints->codec         = stream->codecpar->codec_id;
  hints->extradata     = stream->codecpar->extradata;
  hints->extrasize     = stream->codecpar->extradata_size;
  hints->channels      = stream->codecpar->channels;
  hints->samplerate    = stream->codecpar->sample_rate;
  hints->blockalign    = stream->codecpar->block_align;
  hints->bitrate       = stream->codecpar->bit_rate;
  hints->bitspersample = stream->codecpar->bits_per_coded_sample;
  if(hints->bitspersample == 0)
    hints->bitspersample = 16;

  hints->width         = stream->codecpar->width;
  hints->height        = stream->codecpar->height;
  hints->profile       = stream->codecpar->profile;
  hints->orientation   = 0;

  if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
  {
    hints->fpsrate       = stream->r_frame_rate.num;
    hints->fpsscale      = stream->r_frame_rate.den;

    if(m_bMatroska && stream->avg_frame_rate.den && stream->avg_frame_rate.num)
    {
      hints->fpsrate      = stream->avg_frame_rate.num;
      hints->fpsscale     = stream->avg_frame_rate.den;
    }
    else if(stream->r_frame_rate.num && stream->r_frame_rate.den)
    {
      hints->fpsrate      = stream->r_frame_rate.num;
      hints->fpsscale     = stream->r_frame_rate.den;
    }
    else
    {
      hints->fpsscale     = 0;
      hints->fpsrate      = 0;
    }

    hints->aspect = SelectAspect(stream, hints->forced_aspect) * stream->codecpar->width / stream->codecpar->height;

    AVDictionaryEntry *rtag = av_dict_get(stream->metadata, "rotate", NULL, 0);
    if (rtag)
      hints->orientation = atoi(rtag->value);
    m_aspect = hints->aspect;
    m_width = hints->width;
    m_height = hints->height;
  }

  return true;
}

void OMXReader::GetHints(OMXStreamType type, int index, COMXStreamInfo &hints)
{
  switch (type)
  {
    case OMXSTREAM_AUDIO:
      hints = m_audio_streams[index].hints;
      break;
    case OMXSTREAM_VIDEO:
      hints = m_video_streams[index].hints;
      break;
    case OMXSTREAM_SUBTITLE:
      hints = m_subtitle_streams[index].hints;
      break;
    default:
      break;
  }
}

bool OMXReader::IsEof()
{
  return m_eof;
}

OMXReader::SeekResult OMXReader::SeekChapter(int *chapter, int64_t cur_pts, int64_t* new_pts)
{
  if(cur_pts == AV_NOPTS_VALUE) return SEEK_ERROR;

  // do we have chapters to seek to
  if(m_chapter_count == 0) return SEEK_NO_CHAPTERS;

  // Find current chapter
  int current_chapter = 0;
  for(; current_chapter < m_chapter_count - 1; current_chapter++)
    if(cur_pts >=   m_chapters[current_chapter] && cur_pts <  m_chapters[current_chapter+1])
      break;

  // turn delta into absolute value and check in within range
  int new_chapter = current_chapter + *chapter;
  if(new_chapter < 0 || new_chapter >= m_chapter_count)
    return SEEK_OUT_OF_BOUNDS;

  // convert delta new chapter
  *chapter = new_chapter;

  return SeekTime(m_chapters[new_chapter], *new_pts < cur_pts, new_pts)
    ? SEEK_SUCCESS : SEEK_ERROR;
}

int64_t OMXReader::ConvertTimestamp(int64_t pts, int den, int num)
{
  if(m_pFormatContext == NULL)
    return AV_NOPTS_VALUE;

  if (pts == AV_NOPTS_VALUE)
    return AV_NOPTS_VALUE;

  // do calculations in floats as they can easily overflow otherwise
  // we don't care for having a completly exact timestamp anyway
  double timestamp = (double)pts * (double)num  / (double)den;
  double starttime = 0.0f;

  if (m_pFormatContext->start_time != AV_NOPTS_VALUE)
    starttime = (double)m_pFormatContext->start_time / AV_TIME_BASE;

  if(timestamp > starttime)
    timestamp -= starttime;
  else if( timestamp + 0.1f > starttime )
    timestamp = 0;

  return timestamp * AV_TIME_BASE;
}

void OMXReader::SetSpeed(int iSpeed)
{
  if(!m_pFormatContext)
    return;

  if(m_speed != DVD_PLAYSPEED_PAUSE && iSpeed == DVD_PLAYSPEED_PAUSE)
  {
    av_read_pause(m_pFormatContext);
  }
  else if(m_speed == DVD_PLAYSPEED_PAUSE && iSpeed != DVD_PLAYSPEED_PAUSE)
  {
    av_read_play(m_pFormatContext);
  }
  m_speed = iSpeed;

  AVDiscard discard = AVDISCARD_NONE;
  if(m_speed > 4*DVD_PLAYSPEED_NORMAL)
    discard = AVDISCARD_NONKEY;
  else if(m_speed > 2*DVD_PLAYSPEED_NORMAL)
    discard = AVDISCARD_BIDIR;
  else if(m_speed < DVD_PLAYSPEED_PAUSE)
    discard = AVDISCARD_NONKEY;

  for(unsigned int i = 0; i < m_pFormatContext->nb_streams; i++)
  {
    if(m_pFormatContext->streams[i])
    {
      if(m_pFormatContext->streams[i]->discard != AVDISCARD_ALL)
        m_pFormatContext->streams[i]->discard = discard;
    }
  }
}

int OMXReader::GetStreamLengthSeconds()
{
  if (!m_pFormatContext)
    return 0;

  if(m_DvdPlayer)
    return (int)( m_DvdPlayer->getCurrentTrackLength() / 1000 );

  return (int)(m_pFormatContext->duration / AV_TIME_BASE );
}

int64_t OMXReader::GetStreamLengthMicro()
{
  if (!m_pFormatContext)
    return 0;

  if(m_DvdPlayer)
    return (int64_t)m_DvdPlayer->getCurrentTrackLength() * 1000;

  return m_pFormatContext->duration;
}

double OMXReader::NormalizeFrameduration(double frameduration)
{
  //if the duration is within 20 microseconds of a common duration, use that
  const double durations[] = {AV_TIME_BASE * 1.001 / 24.0, AV_TIME_BASE / 24.0, AV_TIME_BASE / 25.0,
                              AV_TIME_BASE * 1.001 / 30.0, AV_TIME_BASE / 30.0, AV_TIME_BASE / 50.0,
                              AV_TIME_BASE * 1.001 / 60.0, AV_TIME_BASE / 60.0};

  double lowestdiff = AV_TIME_BASE;
  int    selected   = -1;
  for (size_t i = 0; i < sizeof(durations) / sizeof(durations[0]); i++)
  {
    double diff = fabs(frameduration - durations[i]);
    if (diff < DVD_MILLISEC_TO_SEC(0.02) && diff < lowestdiff)
    {
      selected = i;
      lowestdiff = diff;
    }
  }

  if (selected != -1)
    return durations[selected];
  else
    return frameduration;
}

std::string OMXReader::GetStreamCodecName(AVStream *stream)
{
  std::string strStreamName;

  if(!stream)
    return strStreamName;

  unsigned int in = stream->codecpar->codec_tag;
  // FourCC codes are only valid on video streams, audio codecs in AVI/WAV
  // are 2 bytes and audio codecs in transport streams have subtle variation
  // e.g AC-3 instead of ac3
  if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && in != 0)
  {
    char fourcc[5];
    memcpy(fourcc, &in, 4);
    fourcc[4] = 0;
    // fourccs have to be 4 characters
    if (strlen(fourcc) == 4)
    {
      strStreamName = fourcc;
      return strStreamName;
    }
  }

#ifdef FF_PROFILE_DTS_HD_MA
  /* use profile to determine the DTS type */
  if (stream->codecpar->codec_id == AV_CODEC_ID_DTS)
  {
    if (stream->codecpar->profile == FF_PROFILE_DTS_HD_MA)
      strStreamName = "dtshd_ma";
    else if (stream->codecpar->profile == FF_PROFILE_DTS_HD_HRA)
      strStreamName = "dtshd_hra";
    else
      strStreamName = "dca";
    return strStreamName;
  }
#endif

  AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);

  if (codec)
    strStreamName = codec->name;

  return strStreamName;
}

std::string OMXReader::GetCodecName(OMXStreamType type, unsigned int index)
{
  switch(type)
  {
  case OMXSTREAM_AUDIO:
    return m_audio_streams[index].codec_name;
  case OMXSTREAM_VIDEO:
    return m_video_streams[index].codec_name;
  case OMXSTREAM_SUBTITLE:
    return m_subtitle_streams[index].codec_name;
  case OMXSTREAM_NONE:
	break;
  }

  return "";
}

std::string OMXReader::GetStreamLanguage(OMXStreamType type, unsigned int index)
{
  switch(type)
  {
  case OMXSTREAM_AUDIO:
    return m_audio_streams[index].language;
  case OMXSTREAM_VIDEO:
    return m_video_streams[index].language;
  case OMXSTREAM_SUBTITLE:
    return m_subtitle_streams[index].language;
  case OMXSTREAM_NONE:
	break;
  }

  return "";
}

int OMXReader::GetStreamByLanguage(OMXStreamType type, const char *lang)
{
  int count;
  OMXStream *st;

  switch(type)
  {
  case OMXSTREAM_AUDIO:
    st = m_audio_streams;
    count = m_subtitle_count;
    break;
  case OMXSTREAM_VIDEO:
    st = m_video_streams;
    count = m_video_count;
    break;
  case OMXSTREAM_SUBTITLE:
    st = m_subtitle_streams;
    count = m_subtitle_count;
    break;
  case OMXSTREAM_NONE:
	return -1;
  }

  for(int i = 0; i < count; i++)
    if(st[i].language == lang)
      return i;

  return -1;
}

std::string OMXReader::GetStreamMetaData(OMXStreamType type, unsigned int index)
{
  switch(type)
  {
  case OMXSTREAM_AUDIO:
    return m_audio_streams[index].language + ":" +
      m_audio_streams[index].name + ":" +
      m_audio_streams[index].codec_name;
  case OMXSTREAM_VIDEO:
    return m_video_streams[index].language + ":" +
      m_video_streams[index].name + ":" +
      m_video_streams[index].codec_name;
  case OMXSTREAM_SUBTITLE:
    return m_subtitle_streams[index].language + ":" +
      m_subtitle_streams[index].name + ":" +
      m_subtitle_streams[index].codec_name;
  case OMXSTREAM_NONE:
	break;
  }

  return "";
}

bool OMXReader::CanSeek()
{
  if(m_ioContext)
    return m_ioContext->seekable;

  if(!m_pFormatContext || !m_pFormatContext->pb)
    return false;

  if(m_pFormatContext->pb->seekable == AVIO_SEEKABLE_NORMAL)
    return true;

  return false;
}

void get_palette_from_extradata(char *p, char *end, uint32_t **palette_c)
{
  while(p < end) {
    if(strncmp(p, "palette:", 8) == 0) {
      p += 8;
      goto found_palette;
    }
    while(p < end && *p++ != '\n');
  }
  return;

  found_palette:

  char *next;
  uint32_t *palette = *palette_c = (uint32_t *)malloc(sizeof(uint32_t) * 16);
  for(int i = 0; i < 16; i++) {
    palette[i] = strtoul(p, &next, 16);
    if(p == next) {
      free(palette);
      *palette_c = NULL;
      return;
    } else {
      p = next;
    }

    while(*p == ',' || *p == ' ') p++;
  }
}

// Find if dvd subs are present and returns true if so.
// If width and height are available, it sets them too.
// We assume that all dvd subs will have the same dimensions.
bool OMXReader::FindDVDSubs(Dimension &d, float &aspect, uint32_t **palette)
{
  for(int i = 0; i < m_subtitle_count; i++)
  {
    if(m_subtitle_streams[i].hints.codec == AV_CODEC_ID_DVD_SUBTITLE)
    {
      if(m_subtitle_streams[i].hints.width > 0 && m_subtitle_streams[i].hints.height > 0)
      {
        d.width = m_subtitle_streams[i].hints.width;
        d.height = m_subtitle_streams[i].hints.height;
        aspect = d.width / (float)d.height;
      }

      if(*palette == NULL && m_subtitle_streams[i].extrasize > 0)
      {
        get_palette_from_extradata((char *)m_subtitle_streams[i].extradata,
                                   (char *)m_subtitle_streams[i].extradata + m_subtitle_streams[i].extrasize,
                                   palette);
      }
      return true;
    }
  }
  return false;
}

void OMXReader::AddMissingSubtitleStream(int id)
{
  // We've found a new subtitle stream
  AVStream *st = avformat_new_stream(m_pFormatContext, NULL);
  if (!st)
    throw "This isn't meant to happen";
  
  st->id = id;
  st->codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;
  st->codecpar->codec_id = AV_CODEC_ID_DVD_SUBTITLE;
}
