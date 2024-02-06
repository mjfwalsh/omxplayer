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
#include "OMXPacket.h"
#include "OMXClock.h"
#include "omxplayer.h"
#include "utils/defs.h"
#include "utils/log.h"

#include <stdio.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

using namespace std;

std::string OMXReader::s_cookie;
std::string OMXReader::s_user_agent;
std::string OMXReader::s_lavfdopts;
AVDictionary *OMXReader::s_avdict = NULL;

int64_t OMXReader::timeout_start;
int64_t OMXReader::timeout_default_duration = (int64_t)1e10; // amount of time file/network operation can stall for before timing out
int64_t OMXReader::timeout_duration;

OMXPacket::OMXPacket()
:
codec_type(AVMEDIA_TYPE_UNKNOWN),
stream_type_index(-1)
{
  avpkt = av_packet_alloc();
  if(!avpkt) throw "Memory Error";
  
  avpkt->duration = AV_NOPTS_VALUE;
  avpkt->size = 0;
  avpkt->data = NULL;
  avpkt->stream_index = -1;
}


OMXPacket::~OMXPacket()
{
  	av_packet_free(&avpkt);
}

void OMXReader::reset_timeout(int x)
{
  timeout_start = OMXClock::CurrentHostCounter();
  timeout_duration = x * timeout_default_duration;
}


int OMXReader::interrupt_cb(void *unused)
{
  if (timeout_duration && OMXClock::CurrentHostCounter() - timeout_start > timeout_duration)
  {
    CLogLog(LOGERROR, "COMXPlayer::interrupt_cb - Timed out");
    return 1;
  }
  return 0;
}

void OMXReader::SetDefaultTimeout(float timeout)
{
  timeout_default_duration = (int64_t) (timeout * 1e9);
}

void OMXReader::SetCookie(const char *cookie)
{
    if(s_cookie.empty())
    {
        s_cookie.assign(cookie);
    }
    else
    {
        s_cookie.push_back('\n');
        s_cookie.append(cookie);
    }
}

OMXReader::OMXReader()
{
  reset_timeout(3);

  avformat_network_init();

  m_pFormatContext     = avformat_alloc_context();
  if(m_pFormatContext == NULL)
    throw "avformat_alloc_context failed";

  if (av_set_options_string(m_pFormatContext, s_lavfdopts.c_str(), ":", ",") < 0)
    throw "Invalid lavfdopts";

  // set the interrupt callback, appeared in libavformat 53.15.0
  m_pFormatContext->interrupt_callback = { interrupt_cb, NULL };

  // if format can be nonblocking, let's use that
  m_pFormatContext->flags |= AVFMT_FLAG_NONBLOCK;
}

OMXReader::~OMXReader()
{
  avformat_close_input(&m_pFormatContext);
  avformat_network_deinit();
}

OMXPacket *OMXReader::Read()
{
  if(m_eof)
    return NULL;

  // assume we are not eof
  if(m_pFormatContext->pb)
    m_pFormatContext->pb->eof_reached = 0;

  // timeout
  reset_timeout(1);

  // create packet
  OMXPacket *omx_pkt = new OMXPacket();
  if(av_read_frame(m_pFormatContext, omx_pkt->avpkt) < 0 || omx_pkt->avpkt->size < 0 || interrupt_cb())
  {
    delete omx_pkt;
    m_eof = true;
    return NULL;
  }

  AVStream *pStream = m_pFormatContext->streams[omx_pkt->avpkt->stream_index];
  omx_pkt->codec_type = pStream->codecpar->codec_type;

  // let dvd nav streams through
  if(pStream->codecpar->codec_id == AV_CODEC_ID_DVD_NAV)
  {
    omx_pkt->stream_type_index = -1;
    omx_pkt->hints.codec = AV_CODEC_ID_DVD_NAV;
    return omx_pkt;
  }

  try
  {
    omx_pkt->stream_type_index = m_steam_map.at(omx_pkt->avpkt->stream_index);
  }
  catch(std::out_of_range const&)
  {
    // try adding a new subtitle stream
    // AddStream may still return -1 if unsupported
    // dvd sub system may need to be initialised
    if(omx_pkt->codec_type == AVMEDIA_TYPE_SUBTITLE)
    {
      omx_pkt->stream_type_index = AddStream(omx_pkt->avpkt->stream_index);
      if(omx_pkt->stream_type_index == -1)
        pStream->discard = AVDISCARD_ALL;
      else if(m_dvd_subs_need_init)
        initDVDSubs();
    }
    else
    {
      // otherwise ignore it
      pStream->discard = AVDISCARD_ALL;
    }
  }

  if(m_bMatroska && omx_pkt->codec_type == AVMEDIA_TYPE_VIDEO)
  { // matroska can store different timestamps
    // for different formats, for native stored
    // stuff it is pts, but for ms compatibility
    // tracks, it is really dts. sadly ffmpeg
    // sets these two timestamps equal all the
    // time, so we select it here instead
    if(pStream->codecpar->codec_tag == 0)
      omx_pkt->avpkt->dts = AV_NOPTS_VALUE;
    else
      omx_pkt->avpkt->pts = AV_NOPTS_VALUE;
  }

  if(m_bAVI && omx_pkt->codec_type == AVMEDIA_TYPE_VIDEO)
  {
    // AVI's always have borked pts, specially if m_pFormatContext->flags includes
    // AVFMT_FLAG_GENPTS so always use dts
    omx_pkt->avpkt->pts = AV_NOPTS_VALUE;
  }

  SetHints(pStream, &omx_pkt->hints);

  // check if stream has passed full duration, needed for live streams
  // Do this before we convert dts and pts values
  if(omx_pkt->avpkt->dts != (int64_t)AV_NOPTS_VALUE)
  {
    int64_t duration = omx_pkt->avpkt->dts;
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

  omx_pkt->avpkt->dts = ConvertTimestamp(omx_pkt->avpkt->dts, pStream->time_base.den, pStream->time_base.num);
  omx_pkt->avpkt->pts = ConvertTimestamp(omx_pkt->avpkt->pts, pStream->time_base.den, pStream->time_base.num);
  omx_pkt->avpkt->duration = AV_TIME_BASE * omx_pkt->avpkt->duration * pStream->time_base.num / pStream->time_base.den;

  return omx_pkt;
}

int OMXReader::AddStream(int id, const char *lang)
{
  AVStream *pStream = m_pFormatContext->streams[id];
  OMXStreamType type;

  OMXStream *this_stream;
  switch (pStream->codecpar->codec_type)
  {
  case AVMEDIA_TYPE_AUDIO:
    type = OMXSTREAM_AUDIO;
    break;
  case AVMEDIA_TYPE_VIDEO:
    // only allow a single video stream
    // and discard picture attachments (e.g. album art embedded in MP3 or AAC)
    if(m_streams[OMXSTREAM_VIDEO].size() == MAX_VIDEO_STREAMS
        || (pStream->disposition & AV_DISPOSITION_ATTACHED_PIC))
      goto ignore_stream;

    type = OMXSTREAM_VIDEO;
    break;
  case AVMEDIA_TYPE_SUBTITLE:
    switch(pStream->codecpar->codec_id)
    {
    case AV_CODEC_ID_SUBRIP:
    case AV_CODEC_ID_SSA:
    case AV_CODEC_ID_ASS:
    case AV_CODEC_ID_DVD_SUBTITLE:
      break;
    default:
      goto ignore_stream;
    }

    type = OMXSTREAM_SUBTITLE;
    break;
  default:
  ignore_stream:
    pStream->discard = AVDISCARD_ALL;
    return -1;
  }

  m_steam_map[id]          = m_streams[type].size();
  this_stream              = &m_streams[type].emplace_back();
  this_stream->type        = type;

  PopulateStream(id, lang, this_stream);

  // return the stream type index
  return m_steam_map[id];
}

void OMXReader::PopulateStream(int id, const char *lang, OMXStream *this_stream)
{
  AVStream *pStream = m_pFormatContext->streams[id];

  this_stream->hex_id      = pStream->id;
  this_stream->codec_name  = GetStreamCodecName(pStream);
  this_stream->id          = id;
  SetHints(pStream, &this_stream->hints);

  if(lang)
  {
    this_stream->language = lang;
  }
  else
  {
    AVDictionaryEntry *langTag = av_dict_get(pStream->metadata, "language", NULL, 0);
    if (langTag)
      this_stream->language = langTag->value;
  }

  AVDictionaryEntry *titleTag = av_dict_get(pStream->metadata, "title", NULL, 0);
  if (titleTag)
    this_stream->name = titleTag->value;

  if( pStream->codecpar->extradata && pStream->codecpar->extradata_size > 0 )
  {
    this_stream->extrasize = pStream->codecpar->extradata_size;
    this_stream->extradata = pStream->codecpar->extradata;
  }

  // remember the first DVD subtitles stream we come across
  if(this_stream->type == OMXSTREAM_SUBTITLE
      && this_stream->hints.codec == AV_CODEC_ID_DVD_SUBTITLE
      && m_dvd_subs == -1)
  {
    m_dvd_subs = m_steam_map[id];
    m_dvd_subs_need_init = true;
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

bool OMXReader::SetHints(AVStream *stream, COMXStreamInfo *hints)
{
  if(!hints || !stream)
    return false;

  hints->codec         = stream->codecpar->codec_id;
  hints->extradata     = stream->codecpar->extradata;
  hints->extrasize     = stream->codecpar->extradata_size;

#if LIBAVCODEC_VERSION_MAJOR < 59
  hints->channels      = stream->codecpar->channels;
#else
  hints->channels      = stream->codecpar->ch_layout.nb_channels;
#endif

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

COMXStreamInfo OMXReader::GetHints(OMXStreamType type, int index)
{
  return m_streams[type][index].hints;
}

bool OMXReader::IsEof()
{
  return m_eof;
}


int64_t OMXReader::ConvertTimestamp(int64_t pts, int den, int num)
{
  if (pts == AV_NOPTS_VALUE)
    return AV_NOPTS_VALUE;

  // do calculations in floats as they can easily overflow otherwise
  // we don't care for having a completly exact timestamp anyway
  int64_t timestamp = (double)pts * (double)num * (double)AV_TIME_BASE  / (double)den;
  int64_t starttime = 0;

  if (m_pFormatContext->start_time != AV_NOPTS_VALUE)
    starttime = (double)m_pFormatContext->start_time;

  if(timestamp > starttime)
    timestamp -= starttime;
  else if( timestamp + 100000 > starttime )
    timestamp = 0;

  return timestamp;
}

void OMXReader::SetSpeed(float iSpeed)
{
  if(m_speed != DVD_PLAYSPEED_PAUSE && iSpeed == DVD_PLAYSPEED_PAUSE)
    av_read_pause(m_pFormatContext);
  else if(m_speed == DVD_PLAYSPEED_PAUSE && iSpeed != DVD_PLAYSPEED_PAUSE)
    av_read_play(m_pFormatContext);

  m_speed = iSpeed;

  AVDiscard discard = AVDISCARD_NONE;
  if(m_speed > 4.0)
    discard = AVDISCARD_NONKEY;
  else if(m_speed > 2.0)
    discard = AVDISCARD_BIDIR;
  else if(m_speed < 0.0)
    discard = AVDISCARD_NONKEY;

  for(unsigned int i = 0; i < m_pFormatContext->nb_streams; i++)
    if(m_pFormatContext->streams[i]
        && m_pFormatContext->streams[i]->discard != AVDISCARD_ALL)
      m_pFormatContext->streams[i]->discard = discard;
}

int OMXReader::GetStreamLengthSeconds()
{
  return (int)(m_pFormatContext->duration / AV_TIME_BASE );
}

int64_t OMXReader::GetStreamLengthMicro()
{
  return m_pFormatContext->duration;
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

  AVCONST AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);

  if (codec)
    strStreamName = codec->name;

  return strStreamName;
}

std::string OMXReader::GetCodecName(OMXStreamType type, unsigned int index)
{
  return m_streams[type][index].codec_name;
}

std::string OMXReader::GetStreamLanguage(OMXStreamType type, unsigned int index)
{
  return m_streams[type][index].language;
}

int OMXReader::GetStreamByLanguage(OMXStreamType type, const char *lang)
{
  for(int i = 0; i < (int)m_streams[type].size(); i++)
    if(m_streams[type][i].language == lang)
      return i;

  return -1;
}

void OMXReader::GetMetaData(OMXStreamType type, vector<string> &list)
{
  list.resize(m_streams[type].size());

  int i = 0;
  for(const auto &stream : m_streams[type])
  {
    list[i] = to_string(i) + ":" + stream.language + ":" + stream.name + ":" + stream.codec_name + ":";
    i++;
  }
}



// If dvd subs are present returns and available metadata
// We assume that all dvd subs will have the same dimensions.
bool OMXReader::FindDVDSubs(Dimension &d, float &aspect, uint32_t **palette, uint32_t *buf)
{
  if(m_dvd_subs == -1)
    return false;

  OMXStream &dvd_sub_stream = m_streams[OMXSTREAM_SUBTITLE][m_dvd_subs];

  if(dvd_sub_stream.hints.width > 0 && dvd_sub_stream.hints.height > 0)
  {
    d.width = dvd_sub_stream.hints.width;
    d.height = dvd_sub_stream.hints.height;
    aspect = d.width / (float)d.height;
  }

  *palette = getPalette(&dvd_sub_stream, buf);
  m_dvd_subs_need_init = false;

  return true;
}

void OMXReader::info_dump(const std::string &filename)
{
    printf("File: %s\n", filename.c_str());
    printf("Video Streams: %u\n", m_streams[OMXSTREAM_VIDEO].size());
    int i = 0;
    for(const auto &stream : m_streams[OMXSTREAM_VIDEO])
    {
        printf("    %2d. 0x%02x: video: %s (fps %0.2f, size %dx%d, aspect %0.2f)\n",
            ++i,
            stream.hex_id,
            stream.codec_name.c_str(),
            (float)stream.hints.fpsrate / (float)stream.hints.fpsscale,
            stream.hints.width, stream.hints.height,
            stream.hints.aspect);
    }
    printf("Audio Streams: %u\n", m_streams[OMXSTREAM_AUDIO].size());
    i = 0;
    for(const auto &stream : m_streams[OMXSTREAM_AUDIO])
    {
        printf("    %2d. 0x%02x: audio: %s (lang %s, ch %d, sr %d, b/s %d)\n",
            ++i,
            stream.hex_id,
            stream.codec_name.c_str(),
            stream.language.c_str(),
            stream.hints.channels,
            stream.hints.samplerate,
            stream.hints.bitspersample);
    }
    printf("Subtitle Streams: %u\n", m_streams[OMXSTREAM_SUBTITLE].size());
    i = 0;
    for(const auto &stream : m_streams[OMXSTREAM_SUBTITLE])
    {
        printf("    %2d. 0x%02x: subtitle: %s (lang %s)\n",
            ++i,
            stream.hex_id,
            stream.codec_name.c_str(),
            stream.language.c_str());
    }
}

bool OMXReader::SetAvDict(const char *ad)
{
  return av_dict_parse_string(&s_avdict, ad, ":", ",", 0) >= 0;
}

