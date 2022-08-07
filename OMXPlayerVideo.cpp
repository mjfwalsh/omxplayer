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

#include "OMXPlayerVideo.h"

#include <stdio.h>
#include <string>
#include <assert.h>

#include "utils/log.h"

#include "OMXClock.h"
#include "OMXReader.h"
#include "OMXStreamInfo.h"

class Rect;

OMXPlayerVideo::OMXPlayerVideo(OMXClock *av_clock, const OMXVideoConfig &config)
:
m_iCurrentPts(AV_NOPTS_VALUE),
m_flush(false),
m_flush_requested(false),
m_cached_size(0),
m_iVideoDelay(0),
m_config(config)
{
  pthread_cond_init(&m_packet_cond, NULL);
  pthread_mutex_init(&m_lock_decoder, NULL);

  if (m_config.hints.fpsrate && m_config.hints.fpsscale)
    m_fps = AV_TIME_BASE / OMXReader::NormalizeFrameduration((double)AV_TIME_BASE * m_config.hints.fpsscale / m_config.hints.fpsrate);
  else
    m_fps = 25.0;

  m_bAbort = false;

  if( m_fps > 100.0 || m_fps < 5.0 )
  {
    printf("Invalid framerate %f, using forced 25fps and just trust timestamps\n", m_fps);
    m_fps = 25.0;
  }

  m_decoder = new COMXVideo(av_clock, m_config);

  printf("Video codec %s width %d height %d profile %d fps %f\n",
      m_decoder->GetDecoderName(), m_config.hints.width, m_config.hints.height, m_config.hints.profile, m_fps);

  Create();
}

OMXPlayerVideo::~OMXPlayerVideo()
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

  delete m_decoder;

  pthread_cond_destroy(&m_packet_cond);
  pthread_mutex_destroy(&m_lock_decoder);
}

void OMXPlayerVideo::LockDecoder()
{
  pthread_mutex_lock(&m_lock_decoder);
}

void OMXPlayerVideo::UnLockDecoder()
{
  pthread_mutex_unlock(&m_lock_decoder);
}

void OMXPlayerVideo::Reset()
{
  // Quick reset of internal state back to a default that is ready to play from
  // the start or a new position.  This replaces a combination of Close and then
  // Open calls but does away with the DLL unloading/loading, decoder reset, and
  // thread reset.
  Flush();   
  m_iCurrentPts       = AV_NOPTS_VALUE;
  m_flush             = false;
  m_flush_requested   = false;
  m_cached_size       = 0;
  m_iVideoDelay       = 0;
}

void OMXPlayerVideo::SetAlpha(int alpha)
{
  m_decoder->SetAlpha(alpha);
}

void OMXPlayerVideo::SetLayer(int layer)
{
  m_decoder->SetLayer(layer);
}

void OMXPlayerVideo::SetVideoRect(const Rect& SrcRect, const Rect& DestRect)
{
  m_decoder->SetVideoRect(SrcRect, DestRect);
}

void OMXPlayerVideo::SetVideoRect(int aspectMode)
{
  m_decoder->SetVideoRect(aspectMode);
}

bool OMXPlayerVideo::Decode(OMXPacket *pkt)
{
  if (pkt->dts != AV_NOPTS_VALUE)
    pkt->dts += m_iVideoDelay;

  if (pkt->pts != AV_NOPTS_VALUE)
  {
    pkt->pts += m_iVideoDelay;
    m_iCurrentPts = pkt->pts;
  }

  while((int) m_decoder->GetFreeSpace() < pkt->size)
  {
    OMXClock::Sleep(10);
    if(m_flush_requested) return true;
  }

  CLogLog(LOGINFO, "CDVDPlayerVideo::Decode dts:%lld pts:%lld cur:%lld, size:%d", pkt->dts, pkt->pts, m_iCurrentPts, pkt->size);
  m_decoder->Decode(pkt);
  return true;
}

void OMXPlayerVideo::Process()
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
        m_cached_size -= omx_pkt->size;
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

void OMXPlayerVideo::Flush()
{
  m_flush_requested = true;
  Lock();
  LockDecoder();
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
  m_decoder->Reset();
  UnLockDecoder();
  UnLock();
}

bool OMXPlayerVideo::AddPacket(OMXPacket *pkt)
{
  if(m_bAbort)
  {
    delete pkt;
    return true;
  }

  if((m_cached_size + pkt->size) < m_config.queue_size * 1024 * 1024)
  {
    Lock();
    m_cached_size += pkt->size;
    m_packets.push_back(pkt);
    UnLock();
    pthread_cond_broadcast(&m_packet_cond);
    return true;
  }

  return false;
}

int  OMXPlayerVideo::GetDecoderBufferSize()
{
  return m_decoder->GetInputBufferSize();
}

int  OMXPlayerVideo::GetDecoderFreeSpace()
{
  return m_decoder->GetFreeSpace();
}

void OMXPlayerVideo::SubmitEOS()
{
  Lock();
  m_packets.push_back(nullptr);
  UnLock();
  pthread_cond_broadcast(&m_packet_cond);
}

void OMXPlayerVideo::SubmitEOSInternal()
{
  m_decoder->SubmitEOS();
}

bool OMXPlayerVideo::IsEOS()
{
  return m_packets.empty() && m_decoder->IsEOS();
}

