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

#include "OMXAudio.h"
#include "utils/log.h"
#include "OMXClock.h"

#include <assert.h>
#include <algorithm>

extern "C" {
#include <libavutil/channel_layout.h>
}

#define CLASSNAME "COMXAudio"

#ifndef VOLUME_MINIMUM
#define VOLUME_MINIMUM 0
#endif

using namespace std;

// the size of the audio_render output port buffers
#define AUDIO_DECODE_OUTPUT_BUFFER (32*1024)
static const char rounded_up_channels_shift[] = {0,0,1,2,2,3,3,3,3};


bool COMXAudio::PortSettingsChanged()
{
  CSingleLock lock (m_critSection);
  OMX_ERRORTYPE omx_err   = OMX_ErrorNone;

  if (m_settings_changed)
  {
    m_omx_decoder.DisablePort(m_omx_decoder.GetOutputPort(), true);
    m_omx_decoder.EnablePort(m_omx_decoder.GetOutputPort(), true);
    return true;
  }

  if(!m_config.passthrough)
  {
    if(!m_omx_mixer.Initialize("OMX.broadcom.audio_mixer", OMX_IndexParamAudioInit))
      return false;
  }
  if(m_config.device == "omx:both")
  {
    if(!m_omx_splitter.Initialize("OMX.broadcom.audio_splitter", OMX_IndexParamAudioInit))
      return false;
  }
  if (m_config.device == "omx:both" || m_config.device == "omx:local")
  {
    if(!m_omx_render_analog.Initialize("OMX.broadcom.audio_render", OMX_IndexParamAudioInit))
      return false;
  }
  if (m_config.device == "omx:both" || m_config.device == "omx:hdmi")
  {
    if(!m_omx_render_hdmi.Initialize("OMX.broadcom.audio_render", OMX_IndexParamAudioInit))
      return false;
  }
  if (m_config.device == "omx:alsa")
  {
    if(!m_omx_render_analog.Initialize("OMX.alsa.audio_render", OMX_IndexParamAudioInit))
      return false;
  }

  UpdateAttenuation();

  if( m_omx_mixer.IsInitialized() )
  {
    /* setup mixer output */
    OMX_INIT_STRUCTURE(m_pcm_output);
    m_pcm_output.nPortIndex = m_omx_decoder.GetOutputPort();
    omx_err = m_omx_decoder.GetParameter(OMX_IndexParamAudioPcm, &m_pcm_output);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - error m_omx_decoder GetParameter omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }

    memcpy(m_pcm_output.eChannelMapping, m_output_channels, sizeof(m_output_channels));
    // round up to power of 2
    m_pcm_output.nChannels = m_OutputChannels > 4 ? 8 : m_OutputChannels > 2 ? 4 : m_OutputChannels;
    /* limit samplerate (through resampling) if requested */
    m_pcm_output.nSamplingRate = std::min(std::max((int)m_pcm_output.nSamplingRate, 8000), 192000);

    m_pcm_output.nPortIndex = m_omx_mixer.GetOutputPort();
    omx_err = m_omx_mixer.SetParameter(OMX_IndexParamAudioPcm, &m_pcm_output);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - error m_omx_mixer SetParameter omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }

    CLogLog(LOGDEBUG, "%s::%s - Output bps %d samplerate %d channels %d buffer size %d bytes per second %d",
        CLASSNAME, __func__, (int)m_pcm_output.nBitPerSample, (int)m_pcm_output.nSamplingRate, (int)m_pcm_output.nChannels, m_BufferLen, m_BytesPerSec);
    PrintPCM(&m_pcm_output, "output");

    if( m_omx_splitter.IsInitialized() )
    {
      m_pcm_output.nPortIndex = m_omx_splitter.GetInputPort();
      omx_err = m_omx_splitter.SetParameter(OMX_IndexParamAudioPcm, &m_pcm_output);
      if(omx_err != OMX_ErrorNone)
      {
        CLogLog(LOGERROR, "%s::%s - error m_omx_splitter SetParameter omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
        return false;
      }

      m_pcm_output.nPortIndex = m_omx_splitter.GetOutputPort();
      omx_err = m_omx_splitter.SetParameter(OMX_IndexParamAudioPcm, &m_pcm_output);
      if(omx_err != OMX_ErrorNone)
      {
        CLogLog(LOGERROR, "%s::%s - error m_omx_splitter SetParameter omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
        return false;
      }
      m_pcm_output.nPortIndex = m_omx_splitter.GetOutputPort() + 1;
      omx_err = m_omx_splitter.SetParameter(OMX_IndexParamAudioPcm, &m_pcm_output);
      if(omx_err != OMX_ErrorNone)
      {
        CLogLog(LOGERROR, "%s::%s - error m_omx_splitter SetParameter omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
        return false;
      }
    }

    if( m_omx_render_analog.IsInitialized() )
    {
      m_pcm_output.nPortIndex = m_omx_render_analog.GetInputPort();
      omx_err = m_omx_render_analog.SetParameter(OMX_IndexParamAudioPcm, &m_pcm_output);
      if(omx_err != OMX_ErrorNone)
      {
        CLogLog(LOGERROR, "%s::%s - error m_omx_render_analog SetParameter omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
        return false;
      }
    }

    if( m_omx_render_hdmi.IsInitialized() )
    {
      m_pcm_output.nPortIndex = m_omx_render_hdmi.GetInputPort();
      omx_err = m_omx_render_hdmi.SetParameter(OMX_IndexParamAudioPcm, &m_pcm_output);
      if(omx_err != OMX_ErrorNone)
      {
        CLogLog(LOGERROR, "%s::%s - error m_omx_render_hdmi SetParameter omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
        return false;
      }
    }
  }
  if( m_omx_render_analog.IsInitialized() )
  {
    m_omx_tunnel_clock_analog.Initialize(m_omx_clock, m_omx_clock->GetInputPort(),
      &m_omx_render_analog, m_omx_render_analog.GetInputPort()+1);

    omx_err = m_omx_tunnel_clock_analog.Establish();
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - m_omx_tunnel_clock_analog.Establish omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }
    m_omx_render_analog.ResetEos();
  }
  if( m_omx_render_hdmi.IsInitialized() )
  {
    m_omx_tunnel_clock_hdmi.Initialize(m_omx_clock, m_omx_clock->GetInputPort() + (m_omx_render_analog.IsInitialized() ? 2 : 0),
      &m_omx_render_hdmi, m_omx_render_hdmi.GetInputPort()+1);

    omx_err = m_omx_tunnel_clock_hdmi.Establish();
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - m_omx_tunnel_clock_hdmi.Establish omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }
    m_omx_render_hdmi.ResetEos();
  }

  if( m_omx_render_analog.IsInitialized() )
  {
    // By default audio_render is the clock master, and if output samples don't fit the timestamps, it will speed up/slow down the clock.
    // This tends to be better for maintaining audio sync and avoiding audio glitches, but can affect video/display sync
    // when in dual audio mode, make analogue the slave
    OMX_CONFIG_BOOLEANTYPE configBool;
    OMX_INIT_STRUCTURE(configBool);
    configBool.bEnabled = m_config.is_live || m_config.device == "omx:both" ? OMX_FALSE:OMX_TRUE;

    omx_err = m_omx_render_analog.SetConfig(OMX_IndexConfigBrcmClockReferenceSource, &configBool);
    if (omx_err != OMX_ErrorNone)
       return false;

    OMX_CONFIG_BRCMAUDIODESTINATIONTYPE audioDest;
    OMX_INIT_STRUCTURE(audioDest);
    strncpy((char *)audioDest.sName, m_config.device == "omx:alsa" ? m_config.subdevice.c_str() : "local", sizeof(audioDest.sName));
    omx_err = m_omx_render_analog.SetConfig(OMX_IndexConfigBrcmAudioDestination, &audioDest);
    if (omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - m_omx_render_analog.SetConfig omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }
  }

  if( m_omx_render_hdmi.IsInitialized() )
  {
    // By default audio_render is the clock master, and if output samples don't fit the timestamps, it will speed up/slow down the clock.
    // This tends to be better for maintaining audio sync and avoiding audio glitches, but can affect video/display sync
    OMX_CONFIG_BOOLEANTYPE configBool;
    OMX_INIT_STRUCTURE(configBool);
    configBool.bEnabled = m_config.is_live ? OMX_FALSE:OMX_TRUE;

    omx_err = m_omx_render_hdmi.SetConfig(OMX_IndexConfigBrcmClockReferenceSource, &configBool);
    if (omx_err != OMX_ErrorNone)
       return false;

    OMX_CONFIG_BRCMAUDIODESTINATIONTYPE audioDest;
    OMX_INIT_STRUCTURE(audioDest);
    strcpy((char *)audioDest.sName, "hdmi");
    omx_err = m_omx_render_hdmi.SetConfig(OMX_IndexConfigBrcmAudioDestination, &audioDest);
    if (omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - m_omx_render_hdmi.SetConfig omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }
  }

  if( m_omx_splitter.IsInitialized() )
  {
    m_omx_tunnel_splitter_analog.Initialize(&m_omx_splitter, m_omx_splitter.GetOutputPort(), &m_omx_render_analog, m_omx_render_analog.GetInputPort());
    omx_err = m_omx_tunnel_splitter_analog.Establish();
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXAudio::Initialize - Error m_omx_tunnel_splitter_analog.Establish 0x%08x", omx_err);
      return false;
    }

    m_omx_tunnel_splitter_hdmi.Initialize(&m_omx_splitter, m_omx_splitter.GetOutputPort() + 1, &m_omx_render_hdmi, m_omx_render_hdmi.GetInputPort());
    omx_err = m_omx_tunnel_splitter_hdmi.Establish();
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXAudio::Initialize - Error m_omx_tunnel_splitter_hdmi.Establish 0x%08x", omx_err);
      return false;
    }
  }
  if( m_omx_mixer.IsInitialized() )
  {
    m_omx_tunnel_decoder.Initialize(&m_omx_decoder, m_omx_decoder.GetOutputPort(), &m_omx_mixer, m_omx_mixer.GetInputPort());
    if( m_omx_splitter.IsInitialized() )
    {
      m_omx_tunnel_mixer.Initialize(&m_omx_mixer, m_omx_mixer.GetOutputPort(), &m_omx_splitter, m_omx_splitter.GetInputPort());
    }
    else
    {
      if( m_omx_render_analog.IsInitialized() )
      {
        m_omx_tunnel_mixer.Initialize(&m_omx_mixer, m_omx_mixer.GetOutputPort(), &m_omx_render_analog, m_omx_render_analog.GetInputPort());
      }
      if( m_omx_render_hdmi.IsInitialized() )
      {
        m_omx_tunnel_mixer.Initialize(&m_omx_mixer, m_omx_mixer.GetOutputPort(), &m_omx_render_hdmi, m_omx_render_hdmi.GetInputPort());
      }
    }
    CLogLog(LOGDEBUG, "%s::%s - bits:%d mode:%d channels:%d srate:%d nopassthrough", CLASSNAME, __func__,
            (int)m_pcm_input.nBitPerSample, m_pcm_input.ePCMMode, (int)m_pcm_input.nChannels, (int)m_pcm_input.nSamplingRate);
  }
  else
  {
    if( m_omx_render_analog.IsInitialized() )
    {
      m_omx_tunnel_decoder.Initialize(&m_omx_decoder, m_omx_decoder.GetOutputPort(), &m_omx_render_analog, m_omx_render_analog.GetInputPort());
    }
    else if( m_omx_render_hdmi.IsInitialized() )
    {
      m_omx_tunnel_decoder.Initialize(&m_omx_decoder, m_omx_decoder.GetOutputPort(), &m_omx_render_hdmi, m_omx_render_hdmi.GetInputPort());
    }
    CLogLog(LOGDEBUG, "%s::%s - bits:%d mode:%d channels:%d srate:%d passthrough", CLASSNAME, __func__,
            0, 0, 0, 0);
  }

  omx_err = m_omx_tunnel_decoder.Establish();
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "%s::%s - m_omx_tunnel_decoder.Establish omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
    return false;
  }

  if( m_omx_mixer.IsInitialized() )
  {
    omx_err = m_omx_mixer.SetStateForComponent(OMX_StateExecuting);
    if(omx_err != OMX_ErrorNone) {
      CLogLog(LOGERROR, "%s::%s - m_omx_mixer OMX_StateExecuting omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }
  }

  if( m_omx_mixer.IsInitialized() )
  {
    omx_err = m_omx_tunnel_mixer.Establish();
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - m_omx_tunnel_decoder.Establish omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }
  }

  if( m_omx_splitter.IsInitialized() )
  {
    omx_err = m_omx_splitter.SetStateForComponent(OMX_StateExecuting);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - m_omx_splitter OMX_StateExecuting 0x%08x", CLASSNAME, __func__, omx_err);
     return false;
    }
  }
  if( m_omx_render_analog.IsInitialized() )
  {
    omx_err = m_omx_render_analog.SetStateForComponent(OMX_StateExecuting);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - m_omx_render_analog OMX_StateExecuting omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }
  }
  if( m_omx_render_hdmi.IsInitialized() )
  {
    omx_err = m_omx_render_hdmi.SetStateForComponent(OMX_StateExecuting);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - m_omx_render_hdmi OMX_StateExecuting omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      return false;
    }
  }

  m_settings_changed = true;
  return true;
}

static unsigned count_bits(uint64_t value)
{
  unsigned bits = 0;
  for(;value;++bits)
    value &= value - 1;
  return bits;
}

COMXAudio::COMXAudio(OMXClock *clock, const OMXAudioConfig &config, uint64_t channelMap, unsigned int uiBitsPerSample)
  :
  m_CurrentVolume   (0      ),
  m_Mute            (false  ),
  m_BitsPerSample   (uiBitsPerSample),
  m_amplification   (1.0f   ),
  m_attenuation     (1.0f   ),
  m_submitted       (0.0f   ),
  m_eEncoding       (OMX_AUDIO_CodingPCM),
  m_config          (config)
{
  CSingleLock lock (m_critSection);
  OMX_ERRORTYPE omx_err;

  m_InputChannels = count_bits(channelMap);

  if(m_InputChannels == 0)
    throw "COMXAudio Error: No input channels";

  if(m_config.hints.samplerate == 0)
    throw "COMXAudio Error: invalid samplerate";

  m_av_clock = clock;

  if(!m_av_clock)
    throw "COMXAudio Error: bad clock";

  /* passthrough overwrites hw decode */
  if(m_config.passthrough)
  {
    m_config.hwdecode = false;
  }
  else if(m_config.hwdecode)
  {
    /* check again if we are capable to hw decode the format */
    m_config.hwdecode = CanHWDecode(m_config.hints.codec);
  }

  if(m_config.passthrough || m_config.hwdecode)
    SetCodingType(m_config.hints.codec);
  else
    SetCodingType(AV_CODEC_ID_PCM_S16LE);

  m_omx_clock = m_av_clock->GetOMXClock();

  memset(m_input_channels, 0x0, sizeof(m_input_channels));
  memset(m_output_channels, 0x0, sizeof(m_output_channels));
  memset(&m_wave_header, 0x0, sizeof(m_wave_header));

  m_wave_header.Format.nChannels  = 2;
  m_wave_header.dwChannelMask     = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;

  // set the input format, and get the channel layout so we know what we need to open
  if (!m_config.passthrough && channelMap)
  {
    enum PCMChannels inLayout[OMX_AUDIO_MAXCHANNELS];
    enum PCMChannels outLayout[OMX_AUDIO_MAXCHANNELS];
    // force out layout to stereo if input is not multichannel - it gives the receiver a chance to upmix
    if (channelMap == (AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT) || channelMap == AV_CH_FRONT_CENTER)
      m_config.layout = PCM_LAYOUT_2_0;
    BuildChannelMap(&inLayout[0], channelMap);
    m_OutputChannels = BuildChannelMapCEA(&outLayout[0], GetChannelLayout(m_config.layout));

    CPCMRemap m_remap(m_InputChannels, &inLayout[0], m_OutputChannels, &outLayout[0], m_config.layout, m_config.boostOnDownmix);
    m_remap.GetDownmixMatrix(m_downmix_matrix);

    m_wave_header.dwChannelMask = channelMap;
    BuildChannelMapOMX(m_input_channels, channelMap);
    BuildChannelMapOMX(m_output_channels, GetChannelLayout(m_config.layout));
  }

  m_BytesPerSec   = m_config.hints.samplerate * 2 << rounded_up_channels_shift[m_InputChannels];
  m_BufferLen     = m_BytesPerSec * AUDIO_BUFFER_SECONDS;
  unsigned int InputBytesPerSec = m_config.hints.samplerate * m_BitsPerSample * m_InputChannels >> 3;
  m_InputBytesPerMicrosec = (float)InputBytesPerSec / AV_TIME_BASE;

  // should be big enough that common formats (e.g. 6 channel DTS) fit in a single packet.
  // we don't mind less common formats being split (e.g. ape/wma output large frames)
  // 6 channel 32bpp float to 8 channel 16bpp in, so a full 48K input buffer will fit the output buffer
  unsigned int chunkLen = AUDIO_DECODE_OUTPUT_BUFFER * (m_InputChannels * m_BitsPerSample) >> (rounded_up_channels_shift[m_InputChannels] + 4);

  m_wave_header.Samples.wSamplesPerBlock    = 0;
  m_wave_header.Format.nChannels            = m_InputChannels;
  m_wave_header.Format.nBlockAlign          = m_InputChannels *
    (m_BitsPerSample >> 3);
  // 0x8000 is custom format interpreted by GPU as WAVE_FORMAT_IEEE_FLOAT_PLANAR
  m_wave_header.Format.wFormatTag           = m_BitsPerSample == 32 ? 0x8000 : WAVE_FORMAT_PCM;
  m_wave_header.Format.nSamplesPerSec       = m_config.hints.samplerate;
  m_wave_header.Format.nAvgBytesPerSec      = m_BytesPerSec;
  m_wave_header.Format.wBitsPerSample       = m_BitsPerSample;
  m_wave_header.Samples.wValidBitsPerSample = m_BitsPerSample;
  m_wave_header.Format.cbSize               = 0;
  m_wave_header.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;

  if(!m_omx_decoder.Initialize("OMX.broadcom.audio_decode", OMX_IndexParamAudioInit))
    throw "COMXAudio Error: decoder init error";

  OMX_CONFIG_BOOLEANTYPE boolType;
  OMX_INIT_STRUCTURE(boolType);
  boolType.bEnabled = m_config.passthrough ? OMX_TRUE : OMX_FALSE;

  omx_err = m_omx_decoder.SetParameter(OMX_IndexParamBrcmDecoderPassThrough, &boolType);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXAudio::Initialize - Error OMX_IndexParamBrcmDecoderPassThrough 0x%08x", omx_err);
    printf("OMX_IndexParamBrcmDecoderPassThrough omx_err(0x%08x)\n", omx_err);
    throw "COMXAudio Error: OMX_IndexParamBrcmDecoderPassThrough error";
  }

  // set up the number/size of buffers for decoder input
  OMX_PARAM_PORTDEFINITIONTYPE port_param;
  OMX_INIT_STRUCTURE(port_param);
  port_param.nPortIndex = m_omx_decoder.GetInputPort();

  omx_err = m_omx_decoder.GetParameter(OMX_IndexParamPortDefinition, &port_param);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXAudio::Initialize error get OMX_IndexParamPortDefinition (input) omx_err(0x%08x)", omx_err);
    throw "COMXAudio Error: nPortIndex error";
  }

  port_param.format.audio.eEncoding = m_eEncoding;
  port_param.nBufferSize = chunkLen;
  port_param.nBufferCountActual = std::max(port_param.nBufferCountMin, 16U);

  omx_err = m_omx_decoder.SetParameter(OMX_IndexParamPortDefinition, &port_param);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXAudio::Initialize error set OMX_IndexParamPortDefinition (intput) omx_err(0x%08x)", omx_err);
    throw "COMXAudio Error: nBufferCountActual error";
  }

  // set up the number/size of buffers for decoder output
  OMX_INIT_STRUCTURE(port_param);
  port_param.nPortIndex = m_omx_decoder.GetOutputPort();

  omx_err = m_omx_decoder.GetParameter(OMX_IndexParamPortDefinition, &port_param);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXAudio::Initialize error get OMX_IndexParamPortDefinition (output) omx_err(0x%08x)", omx_err);
    throw "COMXAudio Error: nPortIndex error";
  }

  port_param.nBufferCountActual = std::max((unsigned int)port_param.nBufferCountMin, m_BufferLen / port_param.nBufferSize);

  omx_err = m_omx_decoder.SetParameter(OMX_IndexParamPortDefinition, &port_param);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXAudio::Initialize error set OMX_IndexParamPortDefinition (output) omx_err(0x%08x)", omx_err);
    throw "COMXAudio Error: nBufferCountActual error";
  }

  {
    OMX_AUDIO_PARAM_PORTFORMATTYPE formatType;
    OMX_INIT_STRUCTURE(formatType);
    formatType.nPortIndex = m_omx_decoder.GetInputPort();

    formatType.eEncoding = m_eEncoding;

    omx_err = m_omx_decoder.SetParameter(OMX_IndexParamAudioPortFormat, &formatType);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXAudio::Initialize error OMX_IndexParamAudioPortFormat omx_err(0x%08x)", omx_err);
      throw "COMXAudio Error: formatType error";
    }
  }

  omx_err = m_omx_decoder.AllocInputBuffers();
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXAudio::Initialize - Error alloc buffers 0x%08x", omx_err);
    throw "COMXAudio Error: AllocInputBuffers error";
  }

    omx_err = m_omx_decoder.SetStateForComponent(OMX_StateExecuting);
    if(omx_err != OMX_ErrorNone) {
      CLogLog(LOGERROR, "COMXAudio::Initialize - Error setting OMX_StateExecuting 0x%08x", omx_err);
      throw "COMXAudio Error: OMX_StateExecuting error";
    }


  if(m_eEncoding == OMX_AUDIO_CodingPCM)
  {
    OMX_BUFFERHEADERTYPE *omx_buffer = m_omx_decoder.GetInputBuffer();
    if(omx_buffer == NULL)
    {
      CLogLog(LOGERROR, "COMXAudio::Initialize - buffer error 0x%08x", omx_err);
      throw "COMXAudio Error: buffer error";
    }

    omx_buffer->nOffset = 0;
    omx_buffer->nFilledLen  = std::min(sizeof(m_wave_header), omx_buffer->nAllocLen);

    memset((unsigned char *)omx_buffer->pBuffer, 0x0, omx_buffer->nAllocLen);
    memcpy((unsigned char *)omx_buffer->pBuffer, &m_wave_header, omx_buffer->nFilledLen);
    omx_buffer->nFlags = OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;

    omx_err = m_omx_decoder.EmptyThisBuffer(omx_buffer);
    if (omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)", CLASSNAME, __func__, omx_err);
      m_omx_decoder.DecoderEmptyBufferDone(m_omx_decoder.GetComponent(), omx_buffer);
      throw "COMXAudio Error: OMX_EmptyThisBuffer error";
    }
  }
  else if(m_config.hwdecode)
  {
    // send decoder config
    if(m_config.hints.extrasize > 0 && m_config.hints.extradata != NULL)
    {
      OMX_BUFFERHEADERTYPE *omx_buffer = m_omx_decoder.GetInputBuffer();

      if(omx_buffer == NULL)
      {
        CLogLog(LOGERROR, "%s::%s - buffer error 0x%08x", CLASSNAME, __func__, omx_err);
        throw "COMXAudio Error: GetInputBuffer error";
      }

      omx_buffer->nOffset = 0;
      omx_buffer->nFilledLen = std::min((OMX_U32)m_config.hints.extrasize, omx_buffer->nAllocLen);

      memset((unsigned char *)omx_buffer->pBuffer, 0x0, omx_buffer->nAllocLen);
      memcpy((unsigned char *)omx_buffer->pBuffer, m_config.hints.extradata, omx_buffer->nFilledLen);
      omx_buffer->nFlags = OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;

      omx_err = m_omx_decoder.EmptyThisBuffer(omx_buffer);
      if (omx_err != OMX_ErrorNone)
      {
        CLogLog(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)", CLASSNAME, __func__, omx_err);
        m_omx_decoder.DecoderEmptyBufferDone(m_omx_decoder.GetComponent(), omx_buffer);
        throw "COMXAudio Error: EmptyThisBuffer error";
      }
    }
  }

  /* return on decoder error so m_Initialized stays false */
  if(m_omx_decoder.BadState())
    throw "COMXAudio Error: BadState";

  OMX_INIT_STRUCTURE(m_pcm_input);
  m_pcm_input.nPortIndex            = m_omx_decoder.GetInputPort();
  memcpy(m_pcm_input.eChannelMapping, m_input_channels, sizeof(m_input_channels));
  m_pcm_input.eNumData              = OMX_NumericalDataSigned;
  m_pcm_input.eEndian               = OMX_EndianLittle;
  m_pcm_input.bInterleaved          = OMX_TRUE;
  m_pcm_input.nBitPerSample         = m_BitsPerSample;
  m_pcm_input.ePCMMode              = OMX_AUDIO_PCMModeLinear;
  m_pcm_input.nChannels             = m_InputChannels;
  m_pcm_input.nSamplingRate         = m_config.hints.samplerate;

  m_settings_changed = false;
  m_setStartTime  = true;
  m_submitted_eos = false;
  m_failed_eos = false;
  m_last_pts      = AV_NOPTS_VALUE;
  m_submitted     = 0.0f;
  m_maxLevel      = 0.0f;

  CLogLog(LOGDEBUG, "COMXAudio::Initialize Input bps %d samplerate %d channels %d buffer size %d bytes per microsecond %d",
      (int)m_pcm_input.nBitPerSample, (int)m_pcm_input.nSamplingRate, (int)m_pcm_input.nChannels, m_BufferLen, (int)((float)m_InputBytesPerMicrosec * (float)AV_TIME_BASE));
  PrintPCM(&m_pcm_input, "input");
  CLogLog(LOGDEBUG, "COMXAudio::Initialize device %s passthrough %d hwdecode %d",
      m_config.device.c_str(), m_config.passthrough, m_config.hwdecode);
}

//***********************************************************************************************
COMXAudio::~COMXAudio()
{
  CSingleLock lock (m_critSection);

  if ( m_omx_tunnel_clock_analog.IsInitialized() )
    m_omx_tunnel_clock_analog.Deestablish();
  if ( m_omx_tunnel_clock_hdmi.IsInitialized() )
    m_omx_tunnel_clock_hdmi.Deestablish();

  // ignore expected errors on teardown
  if ( m_omx_mixer.IsInitialized() )
    m_omx_mixer.IgnoreNextError(OMX_ErrorPortUnpopulated);
  else
  {
    if ( m_omx_render_hdmi.IsInitialized() )
      m_omx_render_hdmi.IgnoreNextError(OMX_ErrorPortUnpopulated);
    if ( m_omx_render_analog.IsInitialized() )
      m_omx_render_analog.IgnoreNextError(OMX_ErrorPortUnpopulated);
  }

  m_omx_tunnel_decoder.Deestablish();
  if ( m_omx_tunnel_mixer.IsInitialized() )
    m_omx_tunnel_mixer.Deestablish();
  if ( m_omx_tunnel_splitter_hdmi.IsInitialized() )
    m_omx_tunnel_splitter_hdmi.Deestablish();
  if ( m_omx_tunnel_splitter_analog.IsInitialized() )
    m_omx_tunnel_splitter_analog.Deestablish();

  m_omx_decoder.FlushInput();

  m_omx_decoder.Deinitialize();
  if ( m_omx_mixer.IsInitialized() )
    m_omx_mixer.Deinitialize();
  if ( m_omx_splitter.IsInitialized() )
    m_omx_splitter.Deinitialize();
  if ( m_omx_render_hdmi.IsInitialized() )
    m_omx_render_hdmi.Deinitialize();
  if ( m_omx_render_analog.IsInitialized() )
    m_omx_render_analog.Deinitialize();
}

void COMXAudio::Flush()
{
  CSingleLock lock (m_critSection);

  m_omx_decoder.FlushAll();
  if ( m_omx_mixer.IsInitialized() )
    m_omx_mixer.FlushAll();
  if ( m_omx_splitter.IsInitialized() )
    m_omx_splitter.FlushAll();

  if ( m_omx_render_analog.IsInitialized() )
    m_omx_render_analog.FlushAll();
  if ( m_omx_render_hdmi.IsInitialized() )
    m_omx_render_hdmi.FlushAll();

  m_ampqueue.clear();

  if( m_omx_render_analog.IsInitialized() )
    m_omx_render_analog.ResetEos();
  if( m_omx_render_hdmi.IsInitialized() )
    m_omx_render_hdmi.ResetEos();

  m_last_pts      = AV_NOPTS_VALUE;
  m_submitted     = 0.0f;
  m_maxLevel      = 0.0f;
  m_setStartTime  = true;
}

//***********************************************************************************************
void COMXAudio::SetDynamicRangeCompression(long drc)
{
  CSingleLock lock (m_critSection);
  m_amplification = powf(10.0f, (float)drc / 2000.0f);
  if (m_settings_changed)
    UpdateAttenuation();
}

//***********************************************************************************************
void COMXAudio::SetMute(bool bMute)
{
  CSingleLock lock (m_critSection);
  m_Mute = bMute;
  if (m_settings_changed)
    UpdateAttenuation();
}

//***********************************************************************************************
void COMXAudio::SetVolume(float fVolume)
{
  CSingleLock lock (m_critSection);
  m_CurrentVolume = fVolume;
  if (m_settings_changed)
    UpdateAttenuation();
}

float COMXAudio::GetVolume()
{
  return m_Mute ? VOLUME_MINIMUM : m_CurrentVolume;
}

//***********************************************************************************************
bool COMXAudio::ApplyVolume(void)
{
  float m_ac3Gain = 12.0f;
  CSingleLock lock (m_critSection);

  if(m_config.passthrough)
    return false;

  float fVolume = m_Mute ? VOLUME_MINIMUM : m_CurrentVolume;

  // the analogue volume is too quiet for some. Allow use of an advancedsetting to boost this (at risk of distortion) (deprecated)
  double gain = pow(10, (m_ac3Gain - 12.0f) / 20.0);

  const float* coeff = m_downmix_matrix;

  OMX_CONFIG_BRCMAUDIODOWNMIXCOEFFICIENTS8x8 mix;
  OMX_INIT_STRUCTURE(mix);
  OMX_ERRORTYPE omx_err;

  assert(sizeof(mix.coeff)/sizeof(mix.coeff[0]) == 64);

  if (m_amplification != 1.0)
  {
    // reduce scaling so overflow can be seen
    for(size_t i = 0; i < 8*8; ++i)
      mix.coeff[i] = static_cast<unsigned int>(0x10000 * (coeff[i] * gain * 0.01f));

    mix.nPortIndex = m_omx_decoder.GetInputPort();
    omx_err = m_omx_decoder.SetConfig(OMX_IndexConfigBrcmAudioDownmixCoefficients8x8, &mix);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - error setting decoder OMX_IndexConfigBrcmAudioDownmixCoefficients, error 0x%08x",
            CLASSNAME, __func__, omx_err);
      return false;
    }
  }
  for(size_t i = 0; i < 8*8; ++i)
    mix.coeff[i] = static_cast<unsigned int>(0x10000 * (coeff[i] * gain * fVolume * m_amplification * m_attenuation));

  mix.nPortIndex = m_omx_mixer.GetInputPort();
  omx_err = m_omx_mixer.SetConfig(OMX_IndexConfigBrcmAudioDownmixCoefficients8x8, &mix);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "%s::%s - error setting mixer OMX_IndexConfigBrcmAudioDownmixCoefficients, error 0x%08x",
              CLASSNAME, __func__, omx_err);
    return false;
  }
  CLogLog(LOGINFO, "%s::%s - Volume=%.2f (* %.2f * %.2f)", CLASSNAME, __func__, fVolume, m_amplification, m_attenuation);
  return true;
}


//***********************************************************************************************
bool COMXAudio::AddPackets(const void* data, unsigned int len, int64_t dts, int64_t pts, unsigned int frame_size)
{
  CSingleLock lock (m_critSection);

  unsigned pitch = (m_config.passthrough || m_config.hwdecode) ? 1:(m_BitsPerSample >> 3) * m_InputChannels;
  unsigned int demuxer_samples = len / pitch;
  unsigned int demuxer_samples_sent = 0;
  uint8_t *demuxer_content = (uint8_t *)data;

  OMX_ERRORTYPE omx_err;

  OMX_BUFFERHEADERTYPE *omx_buffer = NULL;

  while(demuxer_samples_sent < demuxer_samples)
  {
    // 200ms timeout
    omx_buffer = m_omx_decoder.GetInputBuffer(200);

    if(omx_buffer == NULL)
    {
      CLogLog(LOGERROR, "COMXAudio::Decode timeout");
      puts("COMXAudio::Decode timeout");
      return false;
    }

    omx_buffer->nOffset = 0;
    omx_buffer->nFlags  = 0;

    // we want audio_decode output buffer size to be no more than AUDIO_DECODE_OUTPUT_BUFFER.
    // it will be 16-bit and rounded up to next power of 2 in channels
    unsigned int max_buffer = AUDIO_DECODE_OUTPUT_BUFFER * (m_InputChannels * m_BitsPerSample) >> (rounded_up_channels_shift[m_InputChannels] + 4);

    unsigned int remaining = demuxer_samples-demuxer_samples_sent;
    unsigned int samples_space = std::min(max_buffer, omx_buffer->nAllocLen)/pitch;
    unsigned int samples = std::min(remaining, samples_space);

    omx_buffer->nFilledLen = samples * pitch;

    unsigned int frames = frame_size ? len/frame_size:0;
    if ((samples < demuxer_samples || frames > 1) && m_BitsPerSample==32 && !(m_config.passthrough || m_config.hwdecode))
    {
      const unsigned int sample_pitch   = m_BitsPerSample >> 3;
      const unsigned int frame_samples  = frame_size / pitch;
      const unsigned int plane_size     = frame_samples * sample_pitch;
      const unsigned int out_plane_size = samples * sample_pitch;
      //CLogLog(LOGDEBUG, "%s::%s samples:%d/%d ps:%d ops:%d fs:%d pitch:%d filled:%d frames=%d", CLASSNAME, __func__, samples, demuxer_samples, plane_size, out_plane_size, frame_size, pitch, omx_buffer
      for (unsigned int sample = 0; sample < samples; )
      {
        unsigned int frame = (demuxer_samples_sent + sample) / frame_samples;
        unsigned int sample_in_frame = (demuxer_samples_sent + sample) - frame * frame_samples;
        int out_remaining = std::min(std::min(frame_samples - sample_in_frame, samples), samples-sample);
        uint8_t *src = demuxer_content + frame*frame_size + sample_in_frame * sample_pitch;
        uint8_t *dst = (uint8_t *)omx_buffer->pBuffer + sample * sample_pitch;
        for (unsigned int channel = 0; channel < m_InputChannels; channel++)
        {
          //CLogLog(LOGDEBUG, "%s::%s copy(%d,%d,%d) (s:%d f:%d sin:%d c:%d)", CLASSNAME, __func__, dst-(uint8_t *)omx_buffer->pBuffer, src-demuxer_content, out_remaining, sample, frame, sample_in_frame
          memcpy(dst, src, out_remaining * sample_pitch);
          src += plane_size;
          dst += out_plane_size;
        }
        sample += out_remaining;
      }
    }
    else
    {
       uint8_t *dst = omx_buffer->pBuffer;
       const uint8_t *src = demuxer_content + demuxer_samples_sent * pitch;
       memcpy(dst, src, omx_buffer->nFilledLen);
    }

    int64_t val = pts == AV_NOPTS_VALUE ? 0 : pts;

    if(m_setStartTime)
    {
      omx_buffer->nFlags = OMX_BUFFERFLAG_STARTTIME;

      m_last_pts = pts;

      CLogLog(LOGDEBUG, "COMXAudio::Decode ADec : setStartTime %f", (float)val / AV_TIME_BASE);
      m_setStartTime = false;
    }
    else if(pts == AV_NOPTS_VALUE)
    {
      omx_buffer->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
      m_last_pts = pts;
    }
    else if (pts > m_last_pts)
    {
      m_last_pts = pts;
    }
    else
    {
      omx_buffer->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
    }

    omx_buffer->nTimeStamp = ToOMXTime(val);

    demuxer_samples_sent += samples;

    if(demuxer_samples_sent == demuxer_samples)
      omx_buffer->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

    omx_err = m_omx_decoder.EmptyThisBuffer(omx_buffer);
    if (omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)", CLASSNAME, __func__, omx_err);
      printf("%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
      m_omx_decoder.DecoderEmptyBufferDone(m_omx_decoder.GetComponent(), omx_buffer);
      return false;
    }
    //CLogLog(LOGINFO, "AudiD: dts:%.0f pts:%.0f size:%d", dts, pts, len);

    omx_err = m_omx_decoder.WaitForEvent(OMX_EventPortSettingsChanged, 0);
    if (omx_err == OMX_ErrorNone)
    {
      if(!PortSettingsChanged())
      {
        CLogLog(LOGERROR, "%s::%s - error PortSettingsChanged omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
      }
    }
  }
  m_submitted += (float)demuxer_samples / m_config.hints.samplerate;
  UpdateAttenuation();
  return true;
}

void COMXAudio::UpdateAttenuation()
{
  if (m_amplification == 1.0)
  {
    ApplyVolume();
    return;
  }

  int64_t level_pts = 0;
  float level = GetMaxLevel(level_pts);
  if (level_pts != 0)
  {
    m_ampqueue.emplace_back(level_pts, level);
  }
  int64_t stamp = m_av_clock->GetMediaTime();
  // discard too old data
  while(!m_ampqueue.empty())
  {
    const amplitudes_t &v = m_ampqueue.front();
    /* we'll also consume if queue gets unexpectedly long to avoid filling memory */
    if (v.pts == AV_NOPTS_VALUE || v.pts < stamp || v.pts - stamp > 15000000)
      m_ampqueue.pop_front();
    else break;
  }
  float maxlevel = 0.0f, imminent_maxlevel = 0.0f;
  for(const amplitudes_t &v : m_ampqueue)
  {
    maxlevel = std::max(maxlevel, v.level);
    // check for maximum volume in next 200ms
    if (v.pts != AV_NOPTS_VALUE && v.pts < stamp + 200000)
      imminent_maxlevel = std::max(imminent_maxlevel, v.level);
  }

  if (maxlevel != 0.0)
  {
    float m_limiterHold = 0.025f;
    float m_limiterRelease = 0.100f;

    // if we are going to clip imminently then deal with it now
    if (imminent_maxlevel > m_maxLevel)
    {
      m_maxLevel = imminent_maxlevel;
    // clip but not imminently can ramp up more slowly
    }
    else if (maxlevel > m_maxLevel)
    {
      float alpha_r = -1.0f/(0.100f*log10f(0.900f));
      float attack = powf(10.0f, -1.0f / (alpha_r * m_limiterRelease));
      m_maxLevel = attack * m_maxLevel + (1.0f-attack) * maxlevel;
    // not clipping, decay more slowly
    }
    else
    {
      float alpha_h = -1.0f/(0.025f*log10f(0.999f));
      float decay  = powf(10.0f, -1.0f / (alpha_h * m_limiterHold));
      m_maxLevel = decay  * m_maxLevel + (1.0f-decay ) * maxlevel;
    }

    // want m_maxLevel * amp -> 1.0
    float amp = m_amplification * m_attenuation;

    // We fade in the attenuation over first couple of seconds
    float start = std::min(std::max((m_submitted-1.0f), 0.0f), 1.0f);
    float attenuation = std::min(1.0f, std::max(m_attenuation / (amp * m_maxLevel), 1.0f/m_amplification));
    m_attenuation = (1.0f - start) * 1.0f/m_amplification + start * attenuation;
  }
  else
  {
    m_attenuation = 1.0f/m_amplification;
  }
  ApplyVolume();
}

//***********************************************************************************************
unsigned int COMXAudio::GetSpace()
{
  int free = m_omx_decoder.GetInputBufferSpace();
  return free;
}

int64_t COMXAudio::GetDelay()
{
  CSingleLock lock (m_critSection);
  int64_t stamp = AV_NOPTS_VALUE;
  if (m_last_pts != AV_NOPTS_VALUE && m_av_clock)
    stamp = m_av_clock->GetMediaTime();
  // if possible the delay is current media time - time of last submitted packet
  if (stamp != AV_NOPTS_VALUE)
  {
    return stamp - m_last_pts;
  }
  else // just measure the input fifo
  {
    unsigned int used = m_omx_decoder.GetInputBufferSize() - m_omx_decoder.GetInputBufferSpace();
    return m_InputBytesPerMicrosec ? (float)used / (float)m_InputBytesPerMicrosec : 0.0f;
  }
}

int64_t COMXAudio::GetCacheTime()
{
  return GetDelay();
}

int64_t COMXAudio::GetCacheTotal()
{
  float audioplus_buffer = m_config.hints.samplerate ? 32.0f * 512.0f / m_config.hints.samplerate : 0.0f;
  float input_buffer = m_InputBytesPerMicrosec ? (float)m_omx_decoder.GetInputBufferSize() / (float)m_InputBytesPerMicrosec : 0;
  return AUDIO_BUFFER_SECONDS + input_buffer + audioplus_buffer;
}

//***********************************************************************************************

unsigned int COMXAudio::GetAudioRenderingLatency()
{
  CSingleLock lock (m_critSection);

  OMX_PARAM_U32TYPE param;
  OMX_INIT_STRUCTURE(param);

  if(m_omx_render_analog.IsInitialized())
  {
    param.nPortIndex = m_omx_render_analog.GetInputPort();

    OMX_ERRORTYPE omx_err = m_omx_render_analog.GetConfig(OMX_IndexConfigAudioRenderingLatency, &param);

    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - error getting OMX_IndexConfigAudioRenderingLatency error 0x%08x",
        CLASSNAME, __func__, omx_err);
      return 0;
    }
  }
  else if(m_omx_render_hdmi.IsInitialized())
  {
    param.nPortIndex = m_omx_render_hdmi.GetInputPort();

    OMX_ERRORTYPE omx_err = m_omx_render_hdmi.GetConfig(OMX_IndexConfigAudioRenderingLatency, &param);

    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - error getting OMX_IndexConfigAudioRenderingLatency error 0x%08x",
        CLASSNAME, __func__, omx_err);
      return 0;
    }
  }
  return param.nU32;
}

float COMXAudio::GetMaxLevel(int64_t &pts)
{
  CSingleLock lock (m_critSection);

  OMX_CONFIG_BRCMAUDIOMAXSAMPLE param;
  OMX_INIT_STRUCTURE(param);

  if(m_omx_decoder.IsInitialized())
  {
    param.nPortIndex = m_omx_decoder.GetInputPort();

    OMX_ERRORTYPE omx_err = m_omx_decoder.GetConfig(OMX_IndexConfigBrcmAudioMaxSample, &param);

    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "%s::%s - error getting OMX_IndexConfigBrcmAudioMaxSample error 0x%08x",
        CLASSNAME, __func__, omx_err);
      return 0;
    }
  }
  pts = FromOMXTime(param.nTimeStamp);
  return (float)param.nMaxSample * (100.0f / (1<<15));
}

void COMXAudio::SubmitEOS()
{
  CSingleLock lock (m_critSection);

  m_submitted_eos = true;
  m_failed_eos = false;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *omx_buffer = m_omx_decoder.GetInputBuffer(1000);

  if(omx_buffer == NULL)
  {
    CLogLog(LOGERROR, "%s::%s - buffer error 0x%08x", CLASSNAME, __func__, omx_err);
    m_failed_eos = true;
    return;
  }

  omx_buffer->nOffset     = 0;
  omx_buffer->nFilledLen  = 0;
  omx_buffer->nTimeStamp  = ToOMXTime(0LL);

  omx_buffer->nFlags = OMX_BUFFERFLAG_ENDOFFRAME | OMX_BUFFERFLAG_EOS | OMX_BUFFERFLAG_TIME_UNKNOWN;

  omx_err = m_omx_decoder.EmptyThisBuffer(omx_buffer);
  if (omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)", CLASSNAME, __func__, omx_err);
    m_omx_decoder.DecoderEmptyBufferDone(m_omx_decoder.GetComponent(), omx_buffer);
    return;
  }
  CLogLog(LOGINFO, "%s::%s", CLASSNAME, __func__);
}

bool COMXAudio::IsEOS()
{
  unsigned int latency = GetAudioRenderingLatency();
  CSingleLock lock (m_critSection);

  if (!m_failed_eos && !(m_omx_decoder.IsEOS() && latency == 0))
    return false;

  if (m_submitted_eos)
  {
    CLogLog(LOGINFO, "%s::%s", CLASSNAME, __func__);
    m_submitted_eos = false;
  }
  return true;
}

void COMXAudio::SetCodingType(AVCodecID codec)
{
  switch(codec)
  {
    case AV_CODEC_ID_DTS:
      CLogLog(LOGDEBUG, "COMXAudio::SetCodingType OMX_AUDIO_CodingDTS");
      m_eEncoding = OMX_AUDIO_CodingDTS;
      break;
    case AV_CODEC_ID_AC3:
    case AV_CODEC_ID_EAC3:
      CLogLog(LOGDEBUG, "COMXAudio::SetCodingType OMX_AUDIO_CodingDDP");
      m_eEncoding = OMX_AUDIO_CodingDDP;
      break;
    default:
      CLogLog(LOGDEBUG, "COMXAudio::SetCodingType OMX_AUDIO_CodingPCM");
      m_eEncoding = OMX_AUDIO_CodingPCM;
      break;
  }
}

bool COMXAudio::CanHWDecode(AVCodecID codec)
{
  switch(codec)
  {
    /*
    case AV_CODEC_ID_VORBIS:
      CLogLog(LOGDEBUG, "COMXAudio::CanHWDecode OMX_AUDIO_CodingVORBIS");
      m_eEncoding = OMX_AUDIO_CodingVORBIS;
      m_config.hwdecode = true;
      break;
    case AV_CODEC_ID_AAC:
      CLogLog(LOGDEBUG, "COMXAudio::CanHWDecode OMX_AUDIO_CodingAAC");
      m_eEncoding = OMX_AUDIO_CodingAAC;
      m_config.hwdecode = true;
      break;
    */
    case AV_CODEC_ID_MP2:
    case AV_CODEC_ID_MP3:
      CLogLog(LOGDEBUG, "COMXAudio::CanHWDecode OMX_AUDIO_CodingMP3");
      m_eEncoding = OMX_AUDIO_CodingMP3;
      m_config.hwdecode = true;
      break;
    case AV_CODEC_ID_DTS:
      CLogLog(LOGDEBUG, "COMXAudio::CanHWDecode OMX_AUDIO_CodingDTS");
      m_eEncoding = OMX_AUDIO_CodingDTS;
      m_config.hwdecode = true;
      break;
    case AV_CODEC_ID_AC3:
    case AV_CODEC_ID_EAC3:
      CLogLog(LOGDEBUG, "COMXAudio::CanHWDecode OMX_AUDIO_CodingDDP");
      m_eEncoding = OMX_AUDIO_CodingDDP;
      m_config.hwdecode = true;
      break;
    default:
      CLogLog(LOGDEBUG, "COMXAudio::CanHWDecode OMX_AUDIO_CodingPCM");
      m_eEncoding = OMX_AUDIO_CodingPCM;
      m_config.hwdecode = false;
      break;
  }

  return m_config.hwdecode;
}

bool COMXAudio::HWDecode(AVCodecID codec)
{
  bool ret = false;

  switch(codec)
  {
    /*
    case AV_CODEC_ID_VORBIS:
      CLogLog(LOGDEBUG, "COMXAudio::HWDecode AV_CODEC_ID_VORBIS");
      ret = true;
      break;
    case AV_CODEC_ID_AAC:
      CLogLog(LOGDEBUG, "COMXAudio::HWDecode AV_CODEC_ID_AAC");
      ret = true;
      break;
    */
    case AV_CODEC_ID_MP2:
    case AV_CODEC_ID_MP3:
      CLogLog(LOGDEBUG, "COMXAudio::HWDecode AV_CODEC_ID_MP2 / AV_CODEC_ID_MP3");
      ret = true;
      break;
    case AV_CODEC_ID_DTS:
      CLogLog(LOGDEBUG, "COMXAudio::HWDecode AV_CODEC_ID_DTS");
      ret = true;
      break;
    case AV_CODEC_ID_AC3:
    case AV_CODEC_ID_EAC3:
      CLogLog(LOGDEBUG, "COMXAudio::HWDecode AV_CODEC_ID_AC3 / AV_CODEC_ID_EAC3");
      ret = true;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

void COMXAudio::PrintChannels(const OMX_AUDIO_CHANNELTYPE eChannelMapping[])
{
  for(int i = 0; i < OMX_AUDIO_MAXCHANNELS; i++)
  {
    switch(eChannelMapping[i])
    {
      case OMX_AUDIO_ChannelLF:
        CLogLog(LOGDEBUG, "OMX_AUDIO_ChannelLF");
        break;
      case OMX_AUDIO_ChannelRF:
        CLogLog(LOGDEBUG, "OMX_AUDIO_ChannelRF");
        break;
      case OMX_AUDIO_ChannelCF:
        CLogLog(LOGDEBUG, "OMX_AUDIO_ChannelCF");
        break;
      case OMX_AUDIO_ChannelLS:
        CLogLog(LOGDEBUG, "OMX_AUDIO_ChannelLS");
        break;
      case OMX_AUDIO_ChannelRS:
        CLogLog(LOGDEBUG, "OMX_AUDIO_ChannelRS");
        break;
      case OMX_AUDIO_ChannelLFE:
        CLogLog(LOGDEBUG, "OMX_AUDIO_ChannelLFE");
        break;
      case OMX_AUDIO_ChannelCS:
        CLogLog(LOGDEBUG, "OMX_AUDIO_ChannelCS");
        break;
      case OMX_AUDIO_ChannelLR:
        CLogLog(LOGDEBUG, "OMX_AUDIO_ChannelLR");
        break;
      case OMX_AUDIO_ChannelRR:
        CLogLog(LOGDEBUG, "OMX_AUDIO_ChannelRR");
        break;
      case OMX_AUDIO_ChannelNone:
      case OMX_AUDIO_ChannelKhronosExtensions:
      case OMX_AUDIO_ChannelVendorStartUnused:
      case OMX_AUDIO_ChannelMax:
      default:
        break;
    }
  }
}

void COMXAudio::PrintPCM(OMX_AUDIO_PARAM_PCMMODETYPE *pcm, const char *direction)
{
  CLogLog(LOGDEBUG, "pcm->direction      : %s", direction);
  CLogLog(LOGDEBUG, "pcm->nPortIndex     : %d", (int)pcm->nPortIndex);
  CLogLog(LOGDEBUG, "pcm->eNumData       : %d", pcm->eNumData);
  CLogLog(LOGDEBUG, "pcm->eEndian        : %d", pcm->eEndian);
  CLogLog(LOGDEBUG, "pcm->bInterleaved   : %d", (int)pcm->bInterleaved);
  CLogLog(LOGDEBUG, "pcm->nBitPerSample  : %d", (int)pcm->nBitPerSample);
  CLogLog(LOGDEBUG, "pcm->ePCMMode       : %d", pcm->ePCMMode);
  CLogLog(LOGDEBUG, "pcm->nChannels      : %d", (int)pcm->nChannels);
  CLogLog(LOGDEBUG, "pcm->nSamplingRate  : %d", (int)pcm->nSamplingRate);

  PrintChannels(pcm->eChannelMapping);
}

void COMXAudio::BuildChannelMap(enum PCMChannels *channelMap, uint64_t layout)
{
  int index = 0;
  if (layout & AV_CH_FRONT_LEFT           ) channelMap[index++] = PCM_FRONT_LEFT           ;
  if (layout & AV_CH_FRONT_RIGHT          ) channelMap[index++] = PCM_FRONT_RIGHT          ;
  if (layout & AV_CH_FRONT_CENTER         ) channelMap[index++] = PCM_FRONT_CENTER         ;
  if (layout & AV_CH_LOW_FREQUENCY        ) channelMap[index++] = PCM_LOW_FREQUENCY        ;
  if (layout & AV_CH_BACK_LEFT            ) channelMap[index++] = PCM_BACK_LEFT            ;
  if (layout & AV_CH_BACK_RIGHT           ) channelMap[index++] = PCM_BACK_RIGHT           ;
  if (layout & AV_CH_FRONT_LEFT_OF_CENTER ) channelMap[index++] = PCM_FRONT_LEFT_OF_CENTER ;
  if (layout & AV_CH_FRONT_RIGHT_OF_CENTER) channelMap[index++] = PCM_FRONT_RIGHT_OF_CENTER;
  if (layout & AV_CH_BACK_CENTER          ) channelMap[index++] = PCM_BACK_CENTER          ;
  if (layout & AV_CH_SIDE_LEFT            ) channelMap[index++] = PCM_SIDE_LEFT            ;
  if (layout & AV_CH_SIDE_RIGHT           ) channelMap[index++] = PCM_SIDE_RIGHT           ;
  if (layout & AV_CH_TOP_CENTER           ) channelMap[index++] = PCM_TOP_CENTER           ;
  if (layout & AV_CH_TOP_FRONT_LEFT       ) channelMap[index++] = PCM_TOP_FRONT_LEFT       ;
  if (layout & AV_CH_TOP_FRONT_CENTER     ) channelMap[index++] = PCM_TOP_FRONT_CENTER     ;
  if (layout & AV_CH_TOP_FRONT_RIGHT      ) channelMap[index++] = PCM_TOP_FRONT_RIGHT      ;
  if (layout & AV_CH_TOP_BACK_LEFT        ) channelMap[index++] = PCM_TOP_BACK_LEFT        ;
  if (layout & AV_CH_TOP_BACK_CENTER      ) channelMap[index++] = PCM_TOP_BACK_CENTER      ;
  if (layout & AV_CH_TOP_BACK_RIGHT       ) channelMap[index++] = PCM_TOP_BACK_RIGHT       ;
  while (index<OMX_AUDIO_MAXCHANNELS)
    channelMap[index++] = PCM_INVALID;
}

// See CEA spec: Table 20, Audio InfoFrame data byte 4 for the ordering here
int COMXAudio::BuildChannelMapCEA(enum PCMChannels *channelMap, uint64_t layout)
{
  int index = 0;
  if (layout & AV_CH_FRONT_LEFT           ) channelMap[index++] = PCM_FRONT_LEFT;
  if (layout & AV_CH_FRONT_RIGHT          ) channelMap[index++] = PCM_FRONT_RIGHT;
  if (layout & AV_CH_LOW_FREQUENCY        ) channelMap[index++] = PCM_LOW_FREQUENCY;
  if (layout & AV_CH_FRONT_CENTER         ) channelMap[index++] = PCM_FRONT_CENTER;
  if (layout & AV_CH_BACK_LEFT            ) channelMap[index++] = PCM_BACK_LEFT;
  if (layout & AV_CH_BACK_RIGHT           ) channelMap[index++] = PCM_BACK_RIGHT;
  if (layout & AV_CH_SIDE_LEFT            ) channelMap[index++] = PCM_SIDE_LEFT;
  if (layout & AV_CH_SIDE_RIGHT           ) channelMap[index++] = PCM_SIDE_RIGHT;

  while (index<OMX_AUDIO_MAXCHANNELS)
    channelMap[index++] = PCM_INVALID;

  int num_channels = 0;
  for (index=0; index<OMX_AUDIO_MAXCHANNELS; index++)
    if (channelMap[index] != PCM_INVALID)
       num_channels = index+1;
  // round up to power of 2
  num_channels = num_channels > 4 ? 8 : num_channels > 2 ? 4 : num_channels;
  return num_channels;
}

void COMXAudio::BuildChannelMapOMX(enum OMX_AUDIO_CHANNELTYPE *  channelMap, uint64_t layout)
{
  int index = 0;

  if (layout & AV_CH_FRONT_LEFT           ) channelMap[index++] = OMX_AUDIO_ChannelLF;
  if (layout & AV_CH_FRONT_RIGHT          ) channelMap[index++] = OMX_AUDIO_ChannelRF;
  if (layout & AV_CH_FRONT_CENTER         ) channelMap[index++] = OMX_AUDIO_ChannelCF;
  if (layout & AV_CH_LOW_FREQUENCY        ) channelMap[index++] = OMX_AUDIO_ChannelLFE;
  if (layout & AV_CH_BACK_LEFT            ) channelMap[index++] = OMX_AUDIO_ChannelLR;
  if (layout & AV_CH_BACK_RIGHT           ) channelMap[index++] = OMX_AUDIO_ChannelRR;
  if (layout & AV_CH_SIDE_LEFT            ) channelMap[index++] = OMX_AUDIO_ChannelLS;
  if (layout & AV_CH_SIDE_RIGHT           ) channelMap[index++] = OMX_AUDIO_ChannelRS;
  if (layout & AV_CH_BACK_CENTER          ) channelMap[index++] = OMX_AUDIO_ChannelCS;
  // following are not in openmax spec, but gpu does accept them
  if (layout & AV_CH_FRONT_LEFT_OF_CENTER ) channelMap[index++] = (enum OMX_AUDIO_CHANNELTYPE)10;
  if (layout & AV_CH_FRONT_RIGHT_OF_CENTER) channelMap[index++] = (enum OMX_AUDIO_CHANNELTYPE)11;
  if (layout & AV_CH_TOP_CENTER           ) channelMap[index++] = (enum OMX_AUDIO_CHANNELTYPE)12;
  if (layout & AV_CH_TOP_FRONT_LEFT       ) channelMap[index++] = (enum OMX_AUDIO_CHANNELTYPE)13;
  if (layout & AV_CH_TOP_FRONT_CENTER     ) channelMap[index++] = (enum OMX_AUDIO_CHANNELTYPE)14;
  if (layout & AV_CH_TOP_FRONT_RIGHT      ) channelMap[index++] = (enum OMX_AUDIO_CHANNELTYPE)15;
  if (layout & AV_CH_TOP_BACK_LEFT        ) channelMap[index++] = (enum OMX_AUDIO_CHANNELTYPE)16;
  if (layout & AV_CH_TOP_BACK_CENTER      ) channelMap[index++] = (enum OMX_AUDIO_CHANNELTYPE)17;
  if (layout & AV_CH_TOP_BACK_RIGHT       ) channelMap[index++] = (enum OMX_AUDIO_CHANNELTYPE)18;

  while (index<OMX_AUDIO_MAXCHANNELS)
    channelMap[index++] = OMX_AUDIO_ChannelNone;
}

uint64_t COMXAudio::GetChannelLayout(enum PCMLayout layout)
{
  uint64_t layouts[] = {
    /* 2.0 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT,
    /* 2.1 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT | 1<<PCM_LOW_FREQUENCY,
    /* 3.0 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT | 1<<PCM_FRONT_CENTER,
    /* 3.1 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT | 1<<PCM_FRONT_CENTER | 1<<PCM_LOW_FREQUENCY,
    /* 4.0 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT | 1<<PCM_BACK_LEFT | 1<<PCM_BACK_RIGHT,
    /* 4.1 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT | 1<<PCM_BACK_LEFT | 1<<PCM_BACK_RIGHT | 1<<PCM_LOW_FREQUENCY,
    /* 5.0 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT | 1<<PCM_FRONT_CENTER | 1<<PCM_BACK_LEFT | 1<<PCM_BACK_RIGHT,
    /* 5.1 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT | 1<<PCM_FRONT_CENTER | 1<<PCM_BACK_LEFT | 1<<PCM_BACK_RIGHT | 1<<PCM_LOW_FREQUENCY,
    /* 7.0 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT | 1<<PCM_FRONT_CENTER | 1<<PCM_SIDE_LEFT | 1<<PCM_SIDE_RIGHT | 1<<PCM_BACK_LEFT | 1<<PCM_BACK_RIGHT,
    /* 7.1 */ 1<<PCM_FRONT_LEFT | 1<<PCM_FRONT_RIGHT | 1<<PCM_FRONT_CENTER | 1<<PCM_SIDE_LEFT | 1<<PCM_SIDE_RIGHT | 1<<PCM_BACK_LEFT | 1<<PCM_BACK_RIGHT | 1<<PCM_LOW_FREQUENCY
  };
  return (int)layout < 10 ? layouts[(int)layout] : 0;
}
