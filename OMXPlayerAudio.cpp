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

#include "OMXPlayerAudio.h"
#include "OMXPacket.h"
#include "OMXAudioCodecOMX.h"
#include "OMXClock.h"

#include <stdio.h>
#include <assert.h>

#include "utils/log.h"

OMXPlayerAudio::~OMXPlayerAudio()
{
  m_bAbort  = true;

  Flush();

  if(ThreadHandle())
  {
    Lock();
    pthread_cond_broadcast(&m_packet_cond);
    UnLock();

    StopThread();
  }

  CloseDecoder();
  CloseAudioCodec();

  pthread_cond_destroy(&m_packet_cond);
  pthread_mutex_destroy(&m_lock_decoder);
}

void OMXPlayerAudio::LockDecoder()
{
  pthread_mutex_lock(&m_lock_decoder);
}

void OMXPlayerAudio::UnLockDecoder()
{
  pthread_mutex_unlock(&m_lock_decoder);
}

OMXPlayerAudio::OMXPlayerAudio(OMXClock *av_clock, const OMXAudioConfig &config,
                               std::vector<std::string> &codecs, int active_stream)
:
m_av_clock(av_clock),
m_codecs(codecs),
m_stream_count(codecs.size()),
m_flush_requested(false),
m_config(config)
{
  pthread_cond_init(&m_packet_cond, NULL);
  pthread_mutex_init(&m_lock_decoder, NULL);

  m_bAbort = false;

  if(!SetActiveStream(active_stream))
    throw "OMXPlayerAudio Error: Invalid stream index";

  if(!OpenAudioCodec())
    throw "OMXPlayerAudio Error: Failed to open audio codec";

  if(!OpenDecoder())
    throw "OMXPlayerAudio Error: Failed to open audio decoder";

  Create();
}

int OMXPlayerAudio::GetActiveStream()
{
  return m_stream_index;
}

bool OMXPlayerAudio::SetActiveStream(int new_index)
{
  if(new_index < 0) return false;
  else if(new_index >= m_stream_count) return false;
  else m_stream_index = new_index;

  return true;
}

int OMXPlayerAudio::SetActiveStreamDelta(int delta)
{
  int new_index = m_stream_index + delta;

  // wrap around
  if(new_index < 0) m_stream_index = m_stream_count - 1;
  else if(new_index >= m_stream_count) m_stream_index = 0;
  else m_stream_index = new_index;

  return m_stream_index;
}

bool OMXPlayerAudio::Decode(OMXPacket *pkt)
{
  if(!pkt)
    return false;

  /* last decoder reinit went wrong */
  if(!m_decoder || !m_pAudioCodec)
    return true;

  if(m_stream_index != pkt->stream_type_index)
    return true;

  int channels = pkt->hints.channels;

  unsigned int old_bitrate = m_config.hints.bitrate;
  unsigned int new_bitrate = pkt->hints.bitrate;

  /* only check bitrate changes on AV_CODEC_ID_DTS, AV_CODEC_ID_AC3, AV_CODEC_ID_EAC3 */
  if(m_config.hints.codec != AV_CODEC_ID_DTS && m_config.hints.codec != AV_CODEC_ID_AC3 && m_config.hints.codec != AV_CODEC_ID_EAC3)
  {
    new_bitrate = old_bitrate = 0;
  }

  // for passthrough we only care about the codec and the samplerate
  bool minor_change = channels                 != m_config.hints.channels ||
                      pkt->hints.bitspersample != m_config.hints.bitspersample ||
                      old_bitrate              != new_bitrate;

  if(pkt->hints.codec          != m_config.hints.codec ||
     pkt->hints.samplerate     != m_config.hints.samplerate ||
     (!m_passthrough && minor_change))
  {
    printf("C : %d %d %d %d %d\n", m_config.hints.codec, m_config.hints.channels, m_config.hints.samplerate, m_config.hints.bitrate, m_config.hints.bitspersample);
    printf("N : %d %d %d %d %d\n", pkt->hints.codec, channels, pkt->hints.samplerate, pkt->hints.bitrate, pkt->hints.bitspersample);


    CloseDecoder();
    CloseAudioCodec();

    m_config.hints = pkt->hints;

    m_player_ok = OpenAudioCodec();
    if(!m_player_ok)
      return false;

    m_player_ok = OpenDecoder();
    if(!m_player_ok)
      return false;
  }

  CLogLog(LOGINFO, "CDVDPlayerAudio::Decode dts:%lld pts:%lld size:%d", pkt->avpkt->dts, pkt->avpkt->pts, pkt->avpkt->size);

  if(pkt->avpkt->pts != AV_NOPTS_VALUE)
    m_iCurrentPts = pkt->avpkt->pts;
  else if(pkt->avpkt->dts != AV_NOPTS_VALUE)
    m_iCurrentPts = pkt->avpkt->dts;

  if(!m_passthrough && !m_hw_decode)
  {
    if(!m_pAudioCodec->SendPacket(pkt))
      return true;

    while (m_pAudioCodec->GetFrame()) {
      uint8_t *decoded;
      int decoded_size = m_pAudioCodec->GetData(&decoded, pkt->avpkt->dts, pkt->avpkt->pts);

      if(decoded_size <=0)
        continue;

      while((int) m_decoder->GetSpace() < decoded_size)
      {
        OMXClock::Sleep(10);
        if(m_flush_requested) return true;
      }

      if(!m_decoder->AddPackets(decoded, decoded_size, pkt->avpkt->dts, pkt->avpkt->pts, m_pAudioCodec->GetFrameSize()))
        return false;
    }
  }
  else
  {
    while((int) m_decoder->GetSpace() < pkt->avpkt->size)
    {
      OMXClock::Sleep(10);
      if(m_flush_requested) return true;
    }

    if(!m_decoder->AddPackets(pkt->avpkt->data, pkt->avpkt->size, pkt->avpkt->dts, pkt->avpkt->pts, 0))
      return false;
  }

  return true;
}

void OMXPlayerAudio::Process()
{
  OMXPacket *omx_pkt = NULL;

  while(true)
  {
    Lock();
    if(!m_bAbort && m_packets.empty())
      pthread_cond_wait(&m_packet_cond, &m_lock);

    if (m_bAbort)
    {
      UnLock();
      break;
    }

    if(m_flush && omx_pkt)
    {
      delete omx_pkt;
      omx_pkt = NULL;
      m_flush = false;
    }
    else if(!omx_pkt && !m_packets.empty())
    {
      omx_pkt = m_packets.front();
      if (omx_pkt)
      {
        m_cached_size -= omx_pkt->avpkt->size;
      }
      else
      {
        assert(m_cached_size == 0);
        SubmitEOSInternal();
      }
      m_packets.pop_front();
    }
    UnLock();

    LockDecoder();
    if(m_flush && omx_pkt)
    {
      delete omx_pkt;
      omx_pkt = NULL;
      m_flush = false;
    }
    else if(omx_pkt && Decode(omx_pkt))
    {
      delete omx_pkt;
      omx_pkt = NULL;
    }
    UnLockDecoder();
  }

  if(omx_pkt)
    delete omx_pkt;
}

void OMXPlayerAudio::Flush()
{
  m_flush_requested = true;
  Lock();
  LockDecoder();
  if(m_pAudioCodec)
    m_pAudioCodec->Reset();
  m_flush_requested = false;
  m_flush = true;
  while (!m_packets.empty())
  {
    OMXPacket *pkt = m_packets.front();
    m_packets.pop_front();
    delete pkt;
  }
  m_iCurrentPts = AV_NOPTS_VALUE;
  m_cached_size = 0;
  if(m_decoder)
    m_decoder->Flush();
  UnLockDecoder();
  UnLock();
}

bool OMXPlayerAudio::AddPacket(OMXPacket *pkt)
{
  if(m_bAbort)
  {
    delete pkt;
    return true;
  }

  if((m_cached_size + pkt->avpkt->size) < m_config.queue_size * 1024 * 1024)
  {
    Lock();
    m_cached_size += pkt->avpkt->size;
    m_packets.push_back(pkt);
    UnLock();
    pthread_cond_broadcast(&m_packet_cond);
    return true;
  }

  return false;
}

bool OMXPlayerAudio::OpenAudioCodec()
{
  try {
    m_pAudioCodec = new COMXAudioCodecOMX(m_config.hints, m_config.layout);
  }
  catch(const char *msg) {
    m_pAudioCodec = NULL;
    return false;
  }

  return true;
}

void OMXPlayerAudio::CloseAudioCodec()
{
  if(m_pAudioCodec)
    delete m_pAudioCodec;
  m_pAudioCodec = NULL;
}

bool OMXPlayerAudio::IsPassthrough(COMXStreamInfo hints)
{
  if(m_config.device == "omx:local")
    return false;

  return hints.codec == AV_CODEC_ID_AC3 || hints.codec == AV_CODEC_ID_EAC3
      || hints.codec == AV_CODEC_ID_DTS;
}

bool OMXPlayerAudio::OpenDecoder()
{
  if(m_config.passthrough)
    m_passthrough = IsPassthrough(m_config.hints);

  if(!m_passthrough && m_config.hwdecode)
    m_hw_decode = COMXAudio::HWDecode(m_config.hints.codec);

  if(m_passthrough)
    m_hw_decode = false;

  std::string codec_name = m_codecs[m_stream_index];

  try {
    m_decoder = new COMXAudio(m_av_clock, m_config, m_pAudioCodec->GetChannelMap(), m_pAudioCodec->GetBitsPerSample());
  }
  catch(const char *msg)
  {
  puts(msg);
    m_decoder = NULL;
    return false;
  }

  if(m_passthrough)
  {
    printf("Audio codec %s passthrough channels %d samplerate %d bitspersample %d\n",
      codec_name.c_str(), m_config.hints.channels, m_config.hints.samplerate, m_config.hints.bitspersample);
  }
  else
  {
    printf("Audio codec %s channels %d samplerate %d bitspersample %d\n",
      codec_name.c_str(), m_config.hints.channels, m_config.hints.samplerate, m_config.hints.bitspersample);
  }

  // setup current volume settings
  m_decoder->SetVolume(m_CurrentVolume);
  m_decoder->SetMute(m_mute);
  m_decoder->SetDynamicRangeCompression(m_amplification);

  return true;
}

bool OMXPlayerAudio::CloseDecoder()
{
  if(m_decoder)
    delete m_decoder;
  m_decoder   = NULL;
  return true;
}

int64_t OMXPlayerAudio::GetDelay()
{
  if(m_decoder)
    return m_decoder->GetDelay();
  else
    return 0;
}

int64_t OMXPlayerAudio::GetCacheTime()
{
  if(m_decoder)
    return m_decoder->GetCacheTime();
  else
    return 0;
}

int64_t OMXPlayerAudio::GetCacheTotal()
{
  if(m_decoder)
    return m_decoder->GetCacheTotal();
  else
    return 0;
}

void OMXPlayerAudio::SubmitEOS()
{
  Lock();
  m_packets.push_back(nullptr);
  UnLock();
  pthread_cond_broadcast(&m_packet_cond);
}

void OMXPlayerAudio::SubmitEOSInternal()
{
  if(m_decoder)
    m_decoder->SubmitEOS();
}

bool OMXPlayerAudio::IsEOS()
{
  return m_packets.empty() && (!m_decoder || m_decoder->IsEOS());
}
