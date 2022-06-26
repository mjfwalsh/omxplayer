#pragma once
/*
 *      Copyright (C) 2010 Team XBMC
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

#include "OMXCore.h"
#include "OMXStreamInfo.h"

#include <IL/OMX_Video.h>

#include "utils/simple_geometry.h"
#include "utils/SingleLock.h"

class OMXClock;
class OMXPacket;

#define VIDEO_BUFFERS 60

enum EDEINTERLACEMODE
{
  VS_DEINTERLACEMODE_OFF=0,
  VS_DEINTERLACEMODE_AUTO=1,
  VS_DEINTERLACEMODE_FORCE=2
};

#define CLASSNAME "COMXVideo"

class OMXVideoConfig
{
public:
  COMXStreamInfo hints;
  Rect dst_rect;
  Rect src_rect;
  float display_aspect = 0.0f;
  EDEINTERLACEMODE deinterlace = VS_DEINTERLACEMODE_AUTO;
  bool advanced_hd_deinterlace = true;
  OMX_IMAGEFILTERANAGLYPHTYPE anaglyph = OMX_ImageFilterAnaglyphNone;
  bool hdmi_clock_sync = false;
  bool allow_mvc = false;
  int alpha = 255;
  int aspectMode = 0;
  int display = 0;
  int layer = 0;
  float queue_size = 10.0f;
  float fifo_size = (float)80*1024*60 / (1024*1024);
};

class COMXVideo
{
public:
  COMXVideo(OMXClock *clock, const OMXVideoConfig &config);
  ~COMXVideo();

  // Required overrides
  bool SendDecoderConfig();
  bool NaluFormatStartCodes(enum AVCodecID codec, uint8_t *in_extradata, int in_extrasize);
  bool PortSettingsChanged();
  void PortSettingsChangedLogger(OMX_PARAM_PORTDEFINITIONTYPE port_image, int interlaceEMode);
  unsigned int GetFreeSpace();
  bool  Decode(OMXPacket *pkt);
  void Reset(void);
  std::string GetDecoderName() { return m_video_codec_name; };
  void SetVideoRect(const Rect& SrcRect, const Rect& DestRect);
  void SetVideoRect(int aspectMode);
  void SetVideoRect();
  void SetAlpha(int alpha);
  void SetLayer(int layer);
  int GetInputBufferSize();
  void SubmitEOS();
  bool IsEOS();
  bool SubmittedEOS() { return m_submitted_eos; }
  bool BadState() { return m_omx_decoder.BadState(); };
protected:
  // Video format
  bool              m_drop_state = false;

  OMX_VIDEO_CODINGTYPE m_codingType;

  COMXCoreComponent m_omx_decoder;
  COMXCoreComponent m_omx_render;
  COMXCoreComponent m_omx_sched;
  COMXCoreComponent m_omx_image_fx;
  COMXCoreComponent *m_omx_clock = NULL;
  OMXClock           *m_av_clock = NULL;

  COMXCoreTunel     m_omx_tunnel_decoder;
  COMXCoreTunel     m_omx_tunnel_clock;
  COMXCoreTunel     m_omx_tunnel_sched;
  COMXCoreTunel     m_omx_tunnel_image_fx;

  bool              m_setStartTime = false;

  std::string       m_video_codec_name;

  bool              m_deinterlace = false;
  OMXVideoConfig    m_config;

  float             m_pixel_aspect = 1.0f;
  bool              m_submitted_eos = false;
  bool              m_failed_eos = false;
  OMX_DISPLAYTRANSFORMTYPE m_transform = OMX_DISPLAY_ROT0;
  bool              m_settings_changed = false;
  CCriticalSection  m_critSection;
};
