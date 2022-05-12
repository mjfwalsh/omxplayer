#pragma once

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

#include "DllAvCodec.h"
#include "DllSwResample.h"

#include "utils/PCMRemap.h"

class COMXStreamInfo;
class OMXPacket;

class COMXAudioCodecOMX
{
public:
  COMXAudioCodecOMX(COMXStreamInfo &hints, enum PCMLayout layout);
  ~COMXAudioCodecOMX();
  bool SendPacket(OMXPacket *pkt);
  bool GetFrame();
  int GetData(unsigned char** dst, int64_t &dts, int64_t &pts);
  void Reset();
  uint64_t GetChannelMap();
  int GetBitsPerSample();
  unsigned int GetFrameSize() { return m_frameSize; }

protected:
  AVCodecContext* m_pCodecContext = NULL;
  SwrContext*     m_pConvert = NULL;
  enum AVSampleFormat m_iSampleFormat = AV_SAMPLE_FMT_NONE;
  enum AVSampleFormat m_desiredSampleFormat = AV_SAMPLE_FMT_NONE;

  AVFrame* m_pFrame1 = NULL;

  unsigned char *m_pBufferOutput = NULL;
  int   m_iBufferOutputUsed = 0;
  int   m_iBufferOutputAlloced = 0;

  int     m_channels = 0;

  bool m_bFirstFrame = false;
  bool m_bGotFrame = false;
  bool m_bNoConcatenate = false;
  unsigned int  m_frameSize = 0;
  uint64_t m_dts, m_pts;
};
