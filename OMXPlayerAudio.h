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

#ifndef _OMX_PLAYERAUDIO_H_
#define _OMX_PLAYERAUDIO_H_

extern "C" {
#include <libavformat/avformat.h>
}

#include "OMXStreamInfo.h"
#include "OMXAudio.h"
#include "OMXThread.h"

#include <list>
#include <vector>
#include <string>
#include <atomic>
#include <stdint.h>

class COMXAudioCodecOMX;
class OMXClock;
class OMXPacket;
class OMXReader;

class OMXPlayerAudio : public OMXThread
{
protected:
  std::list<OMXPacket *>    m_packets;
  int64_t                   m_iCurrentPts        = AV_NOPTS_VALUE;
  pthread_cond_t            m_packet_cond;
  pthread_mutex_t           m_lock_decoder;
  OMXClock                  *m_av_clock;
  std::vector<std::string>  m_codecs;
  COMXAudio                 *m_decoder           = NULL;
  std::atomic<int>          m_stream_index;
  int                       m_stream_count;
  bool                      m_passthrough        = false;
  bool                      m_hw_decode          = false;
  bool                      m_flush              = false;
  std::atomic<bool>         m_flush_requested;
  unsigned int              m_cached_size        = 0;
  OMXAudioConfig            m_config;
  COMXAudioCodecOMX         *m_pAudioCodec       = NULL;
  float                     m_CurrentVolume      = 1.0f;
  long                      m_amplification      = 0;
  bool                      m_mute               = false;
  bool                      m_player_ok          = true;

  void LockDecoder();
  void UnLockDecoder();
private:
public:
  OMXPlayerAudio(OMXClock *av_clock, const OMXAudioConfig &config,
    std::vector<std::string> &codecs, int active_stream);

  ~OMXPlayerAudio() override;

  void Flush();
  int GetActiveStream();
  bool SetActiveStream(int new_index);
  int SetActiveStreamDelta(int delta);
  bool AddPacket(OMXPacket *pkt);
  int64_t GetDelay();
  int64_t GetCacheTime();
  int64_t GetCacheTotal();
  int64_t GetCurrentPTS() { return m_iCurrentPts; };
  void SubmitEOS();
  bool IsEOS();
  unsigned int GetCached() { return m_cached_size; };
  void SetVolume(float fVolume)                          { m_CurrentVolume = fVolume; if(m_decoder) m_decoder->SetVolume(fVolume); }
  float GetVolume()                                      { return m_CurrentVolume; }
  void SetMute(bool bOnOff)                              { m_mute = bOnOff; if(m_decoder) m_decoder->SetMute(bOnOff); }
  bool GetMute()                                         { return m_mute; }
  void SetDynamicRangeCompression(long drc)              { m_amplification = drc; if(m_decoder) m_decoder->SetDynamicRangeCompression(drc); }
  bool Error() { return !m_player_ok; };
private:
  void SubmitEOSInternal();
  bool Decode(OMXPacket *pkt);
  void Process() override;
  bool OpenAudioCodec();
  void CloseAudioCodec();
  bool IsPassthrough(COMXStreamInfo hints);
  bool OpenDecoder();
  bool CloseDecoder();
};
#endif
