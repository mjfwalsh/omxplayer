/*
* XBMC Media Center
* Copyright (c) 2002 d7o3g4q and RUNTiME
* Portions Copyright (c) by the authors of ffmpeg and xvid
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

//////////////////////////////////////////////////////////////////////

#ifndef __OPENMAXAUDIORENDER_H__
#define __OPENMAXAUDIORENDER_H__

#include <string>
#include <list>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "linux/PlatformDefs.h"
#include "utils/PCMRemap.h"
#include "OMXCore.h"
#include "OMXStreamInfo.h"
#include "utils/SingleLock.h"

class OMXClock;

#define AUDIO_BUFFER_SECONDS 3

class OMXAudioConfig
{
public:
  COMXStreamInfo hints;
  std::string device;
  std::string subdevice;
  enum PCMLayout layout = PCM_LAYOUT_2_0;
  bool boostOnDownmix = true;
  bool passthrough = false;
  bool hwdecode = false;
  bool is_live = false;
  float queue_size = 3.0f;
  float fifo_size = 2.0f;
};

class COMXAudio
{
public:
  int64_t GetDelay();
  int64_t GetCacheTime();
  int64_t GetCacheTotal();

  COMXAudio(OMXClock *clock, const OMXAudioConfig &config, uint64_t channelMap, unsigned int uiBitsPerSample);
  ~COMXAudio();

  bool AddPackets(const void* data, unsigned int len, int64_t dts, int64_t pts, unsigned int frame_size);
  unsigned int GetSpace();

  void SetVolume(float nVolume);
  float GetVolume();
  void SetMute(bool bOnOff);
  void SetDynamicRangeCompression(long drc);

  void SubmitEOS();
  bool IsEOS();
  void Flush();
  static bool HWDecode(AVCodecID codec);

private:
  bool PortSettingsChanged();
  float GetMaxLevel(int64_t &pts);
  unsigned int GetAudioRenderingLatency();
  bool ApplyVolume();
  void SetCodingType(AVCodecID codec);
  bool CanHWDecode(AVCodecID codec);
  void PrintChannels(const OMX_AUDIO_CHANNELTYPE eChannelMapping[]);
  void PrintPCM(OMX_AUDIO_PARAM_PCMMODETYPE *pcm, const char *direction);
  void UpdateAttenuation();
  void BuildChannelMap(enum PCMChannels *channelMap, uint64_t layout);
  int BuildChannelMapCEA(enum PCMChannels *channelMap, uint64_t layout);
  void BuildChannelMapOMX(enum OMX_AUDIO_CHANNELTYPE *channelMap, uint64_t layout);
  uint64_t GetChannelLayout(enum PCMLayout layout);

  float         m_CurrentVolume;
  bool          m_Mute;
  unsigned int  m_BytesPerSec;
  float         m_InputBytesPerMicrosec;
  unsigned int  m_BufferLen;
  unsigned int  m_InputChannels;
  unsigned int  m_OutputChannels;
  unsigned int  m_BitsPerSample;
  float    m_maxLevel;
  float         m_amplification;
  float         m_attenuation;
  float         m_submitted;
  COMXCoreComponent *m_omx_clock;
  OMXClock      *m_av_clock;
  bool          m_settings_changed;
  bool          m_setStartTime;
  OMX_AUDIO_CODINGTYPE m_eEncoding;
  int64_t      m_last_pts;
  bool          m_submitted_eos;
  bool          m_failed_eos;
  OMXAudioConfig m_config;

  OMX_AUDIO_CHANNELTYPE m_input_channels[OMX_AUDIO_MAXCHANNELS];
  OMX_AUDIO_CHANNELTYPE m_output_channels[OMX_AUDIO_MAXCHANNELS];
  OMX_AUDIO_PARAM_PCMMODETYPE m_pcm_output;
  OMX_AUDIO_PARAM_PCMMODETYPE m_pcm_input;
  OMX_AUDIO_PARAM_DTSTYPE     m_dtsParam;
  WAVEFORMATEXTENSIBLE        m_wave_header;
  class amplitudes_t {
		public:
    amplitudes_t(int64_t p, float l) : pts(p), level(l) {}

    int64_t pts;
    float level;
  };
  std::list<amplitudes_t> m_ampqueue;
  float m_downmix_matrix[OMX_AUDIO_MAXCHANNELS*OMX_AUDIO_MAXCHANNELS];

protected:
  COMXCoreComponent m_omx_render_analog;
  COMXCoreComponent m_omx_render_hdmi;
  COMXCoreComponent m_omx_splitter;
  COMXCoreComponent m_omx_mixer;
  COMXCoreComponent m_omx_decoder;
  COMXCoreTunel     m_omx_tunnel_clock_analog;
  COMXCoreTunel     m_omx_tunnel_clock_hdmi;
  COMXCoreTunel     m_omx_tunnel_mixer;
  COMXCoreTunel     m_omx_tunnel_decoder;
  COMXCoreTunel     m_omx_tunnel_splitter_analog;
  COMXCoreTunel     m_omx_tunnel_splitter_hdmi;
  CCriticalSection m_critSection;
};
#endif
