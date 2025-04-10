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

#ifndef _OMX_PLAYERVIDEO_H_
#define _OMX_PLAYERVIDEO_H_

#include "OMXVideo.h"
#include "OMXThread.h"

#include <list>
#include <atomic>

class OMXClock;
class OMXPacket;
struct AVStream;

class OMXPlayerVideo : public OMXThread
{
protected:
  std::list<OMXPacket *>    m_packets;
  int64_t                   m_iCurrentPts = 0;
  pthread_cond_t            m_packet_cond;
  pthread_mutex_t           m_lock_decoder;
  COMXVideo                 *m_decoder = NULL;
  float                     m_fps = 25.0f;
  bool                      m_flush = false;
  std::atomic<bool>         m_flush_requested;
  unsigned int              m_cached_size = 0;
  int64_t                   m_iVideoDelay = 0;
  OMXVideoConfig            m_config;

  void LockDecoder();
  void UnLockDecoder();
public:
  OMXPlayerVideo(OMXClock *av_clock, const OMXVideoConfig &config);
  ~OMXPlayerVideo() override;
  void Reset();

  void Flush();
  bool AddPacket(OMXPacket *pkt);
  int  GetDecoderBufferSize();
  int  GetDecoderFreeSpace();
  int64_t GetCurrentPTS() { return m_iCurrentPts; };
  unsigned int GetCached() { return m_cached_size; };
  void SubmitEOS();
  bool IsEOS();
  void SetDelay(int64_t delay) { m_iVideoDelay = delay; }
  int64_t GetDelay() { return m_iVideoDelay; }
  void SetAlpha(int alpha);
  void SetLayer(int layer);
  void SetVideoRect(const Rect& SrcRect, const Rect& DestRect);
  void SetVideoRect(int aspectMode);
  static double NormalizeFrameduration(double frameduration);

private:
  void Process() override;
  void Decode(OMXPacket *pkt);
  void SubmitEOSInternal();
  OMXPlayerVideo(const OMXPlayerVideo&) = delete;
  OMXPlayerVideo& operator=(const OMXPlayerVideo&) = delete;
};
#endif
