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

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include "OMXAudioCodecOMX.h"
#include "OMXPacket.h"
#include "utils/defs.h"
#include "utils/log.h"
#include "utils/PCMRemap.h"

#if LIBAVCODEC_VERSION_MAJOR < 59
    #define NB_CHANNELS channels
#else
    #define NB_CHANNELS ch_layout.nb_channels
#endif

// the size of the audio_render output port buffers
#define AUDIO_DECODE_OUTPUT_BUFFER (32*1024)
static const char rounded_up_channels_shift[] = {0,0,1,2,2,3,3,3,3};

COMXAudioCodecOMX::COMXAudioCodecOMX(COMXStreamInfo &hints)
{
  AVCONST AVCodec* pCodec;

  pCodec = avcodec_find_decoder(hints.codec);
  if (!pCodec)
  {
    CLogLog(LOGDEBUG,"COMXAudioCodecOMX::Open() Unable to find codec %d", hints.codec);
    throw "Failed to open audio codec";
  }

  m_bFirstFrame = true;
  m_pCodecContext = avcodec_alloc_context3(pCodec);
  if (!m_pCodecContext)
  {
    CLogLog(LOGDEBUG,"COMXAudioCodecOMX::Open() Failed to allocate codec context");
    throw "Failed to allocate codec context";
  }

  m_pCodecContext->debug = 0;
  m_pCodecContext->workaround_bugs = 1;

  m_channels = 0;
  m_pCodecContext->NB_CHANNELS = hints.channels;
  m_pCodecContext->sample_rate = hints.samplerate;
  m_pCodecContext->block_align = hints.blockalign;
  m_pCodecContext->bit_rate = hints.bitrate;
  m_pCodecContext->bits_per_coded_sample = hints.bitspersample;

  if(m_pCodecContext->bits_per_coded_sample == 0)
    m_pCodecContext->bits_per_coded_sample = 16;

  if( hints.extradata && hints.extrasize > 0 )
  {
    m_pCodecContext->extradata_size = hints.extrasize;
    m_pCodecContext->extradata = (uint8_t*)av_mallocz(hints.extrasize + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(m_pCodecContext->extradata, hints.extradata, hints.extrasize);
  }

  if (avcodec_open2(m_pCodecContext, pCodec, nullptr) < 0)
  {
    CLogLog(LOGDEBUG,"COMXAudioCodecOMX::Open() Unable to open codec");
    avcodec_free_context(&m_pCodecContext);
    throw "Failed to open audio codec";
  }

  m_pFrame1 = av_frame_alloc();
  m_iSampleFormat = AV_SAMPLE_FMT_NONE;
  m_desiredSampleFormat = m_pCodecContext->sample_fmt == AV_SAMPLE_FMT_S16 ? AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_FLTP;
}

COMXAudioCodecOMX::~COMXAudioCodecOMX()
{
  if (m_pBufferOutput)
    av_free(m_pBufferOutput);

  if (m_pFrame1)
    av_frame_free(&m_pFrame1);

  if (m_pConvert)
    swr_free(&m_pConvert);

  avcodec_free_context(&m_pCodecContext);
}

bool COMXAudioCodecOMX::SendPacket(OMXPacket *pkt)
{
  if (!m_iBufferOutputUsed)
  {
    m_dts = pkt->avpkt->dts;
    m_pts = pkt->avpkt->pts;
  }

  int result = avcodec_send_packet(m_pCodecContext, pkt->avpkt);

  if (result && m_bFirstFrame)
    CLogLog(LOGDEBUG, "COMXAudioCodecOMX::SendPacket(%p,%d)", pkt->avpkt->data, pkt->avpkt->size);

  return result == 0;
}


bool COMXAudioCodecOMX::GetFrame()
{
  if (m_bGotFrame)
    return true;

  int result = avcodec_receive_frame(m_pCodecContext, m_pFrame1);
  if (result == 0)
  {
    m_bGotFrame = true;

    if (m_bFirstFrame)
    {
      CLogLog(LOGDEBUG, "COMXAudioCodecOMX::GetFrame format=%d(%d) chan=%d samples=%d size=%d data=%p,%p,%p,%p,%p,%p,%p,%p",
               m_pCodecContext->sample_fmt, m_desiredSampleFormat, m_pCodecContext->NB_CHANNELS, m_pFrame1->nb_samples,
               m_pFrame1->linesize[0],
               m_pFrame1->data[0], m_pFrame1->data[1], m_pFrame1->data[2], m_pFrame1->data[3], m_pFrame1->data[4], m_pFrame1->data[5], m_pFrame1->data[6], m_pFrame1->data[7]
               );
    }
    return true;
  }
  else if(result == AVERROR_EOF || result == AVERROR(EAGAIN))
  {
    return false;
  }
  else
  {
    CLogLog(LOGDEBUG, "COMXAudioCodecOMX::GetFrame error=%d", result);
    Reset();
    return false;
  }
}

int COMXAudioCodecOMX::GetData(unsigned char** dst, int64_t &dts, int64_t &pts)
{
  if (!m_bGotFrame)
    return 0;
  int inLineSize, outLineSize;
  /* input audio is aligned */
  int inputSize = av_samples_get_buffer_size(&inLineSize, m_pCodecContext->NB_CHANNELS, m_pFrame1->nb_samples, m_pCodecContext->sample_fmt, 0);
  /* output audio will be packed */
  int outputSize = av_samples_get_buffer_size(&outLineSize, m_pCodecContext->NB_CHANNELS, m_pFrame1->nb_samples, m_desiredSampleFormat, 1);

  if (!m_bNoConcatenate && m_iBufferOutputUsed && (int)m_frameSize != outputSize)
  {
    CLogLog(LOGERROR, "COMXAudioCodecOMX::GetData Unexpected change of size (%d->%d)", m_frameSize, outputSize);
    m_bNoConcatenate = true;
  }

  // if this buffer won't fit then flush out what we have
  int desired_size = AUDIO_DECODE_OUTPUT_BUFFER * (m_pCodecContext->NB_CHANNELS * GetBitsPerSample()) >> (rounded_up_channels_shift[m_pCodecContext->NB_CHANNELS] + 4);
  if (m_iBufferOutputUsed && (m_iBufferOutputUsed + outputSize > desired_size || m_bNoConcatenate))
  {
     int ret = m_iBufferOutputUsed;
     m_iBufferOutputUsed = 0;
     m_bNoConcatenate = false;
     dts = m_dts;
     pts = m_pts;
     *dst = m_pBufferOutput;
     return ret;
  }
  m_frameSize = outputSize;

  if (m_iBufferOutputAlloced < m_iBufferOutputUsed + outputSize)
  {
     m_pBufferOutput = (unsigned char*)av_realloc(m_pBufferOutput, m_iBufferOutputUsed + outputSize + AV_INPUT_BUFFER_PADDING_SIZE);
     m_iBufferOutputAlloced = m_iBufferOutputUsed + outputSize;
  }

  /* need to convert format */
  if(m_pCodecContext->sample_fmt != m_desiredSampleFormat)
  {
    if(m_pConvert && (m_pCodecContext->sample_fmt != m_iSampleFormat || m_channels != m_pCodecContext->NB_CHANNELS))
    {
      swr_free(&m_pConvert);
      m_channels = m_pCodecContext->NB_CHANNELS;
    }

    if(!m_pConvert)
    {
      m_iSampleFormat = m_pCodecContext->sample_fmt;

#if LIBAVCODEC_VERSION_MAJOR < 59
      m_pConvert = swr_alloc_set_opts(nullptr,
                      av_get_default_channel_layout(m_pCodecContext->channels),
                      m_desiredSampleFormat, m_pCodecContext->sample_rate,
                      av_get_default_channel_layout(m_pCodecContext->channels),
                      m_pCodecContext->sample_fmt, m_pCodecContext->sample_rate,
                      0, nullptr);
#else
      av_channel_layout_default(&m_pCodecContext->ch_layout, m_pCodecContext->NB_CHANNELS);
      swr_alloc_set_opts2(&m_pConvert,
                      &m_pCodecContext->ch_layout,
                      m_desiredSampleFormat, m_pCodecContext->sample_rate,
                      &m_pCodecContext->ch_layout,
                      m_pCodecContext->sample_fmt, m_pCodecContext->sample_rate,
                      0, nullptr);
#endif

      if(!m_pConvert || swr_init(m_pConvert) < 0)
      {
        CLogLog(LOGERROR, "COMXAudioCodecOMX::Decode - Unable to initialise convert format %d to %d", m_pCodecContext->sample_fmt, m_desiredSampleFormat);
        return 0;
      }
    }

    /* use unaligned flag to keep output packed */
    uint8_t *out_planes[m_pCodecContext->NB_CHANNELS];
    if(av_samples_fill_arrays(out_planes, nullptr, m_pBufferOutput + m_iBufferOutputUsed, m_pCodecContext->NB_CHANNELS, m_pFrame1->nb_samples, m_desiredSampleFormat, 1) < 0 ||
       swr_convert(m_pConvert, out_planes, m_pFrame1->nb_samples, (const uint8_t **)m_pFrame1->data, m_pFrame1->nb_samples) < 0)
    {
      CLogLog(LOGERROR, "COMXAudioCodecOMX::Decode - Unable to convert format %d to %d", (int)m_pCodecContext->sample_fmt, m_desiredSampleFormat);
      outputSize = 0;
    }
  }
  else
  {
    /* copy to a contiguous buffer */
    uint8_t *out_planes[m_pCodecContext->NB_CHANNELS];
    if (av_samples_fill_arrays(out_planes, nullptr, m_pBufferOutput + m_iBufferOutputUsed, m_pCodecContext->NB_CHANNELS, m_pFrame1->nb_samples, m_desiredSampleFormat, 1) < 0 ||
      av_samples_copy(out_planes, m_pFrame1->data, 0, 0, m_pFrame1->nb_samples, m_pCodecContext->NB_CHANNELS, m_desiredSampleFormat) < 0 )
    {
      outputSize = 0;
    }
  }
  m_bGotFrame = false;

  if (m_bFirstFrame)
  {
    CLogLog(LOGDEBUG, "COMXAudioCodecOMX::GetData size=%d/%d line=%d/%d buf=%p, desired=%d", inputSize, outputSize, inLineSize, outLineSize, m_pBufferOutput, desired_size);
    m_bFirstFrame = false;
  }
  m_iBufferOutputUsed += outputSize;
  return 0;
}

void COMXAudioCodecOMX::Reset()
{
  avcodec_flush_buffers(m_pCodecContext);
  m_bGotFrame = false;
  m_iBufferOutputUsed = 0;
}

int COMXAudioCodecOMX::GetBitsPerSample()
{
  return m_pCodecContext->sample_fmt == AV_SAMPLE_FMT_S16 ? 16 : 32;
}

#if LIBAVCODEC_VERSION_MAJOR < 59
uint64_t COMXAudioCodecOMX::GetChannelMap()
{
  uint64_t layout;
  int bits = CPCMRemap::CountBits(m_pCodecContext->channel_layout);
  if (bits == m_pCodecContext->channels)
  {
    layout = m_pCodecContext->channel_layout;
  }
  else
  {
    CLogLog(LOGINFO, "COMXAudioCodecOMX::GetChannelMap - FFmpeg reported %d channels, but the layout contains %d ignoring", m_pCodecContext->channels, bits);
    layout = av_get_default_channel_layout(m_pCodecContext->channels);
  }

  return layout;
}
#else
uint64_t COMXAudioCodecOMX::GetChannelMap()
{
  bool tryDefault = true;
  int bits = -1;

  retry:
  if(m_pCodecContext->ch_layout.order == AV_CHANNEL_ORDER_NATIVE
      || m_pCodecContext->ch_layout.order == AV_CHANNEL_ORDER_AMBISONIC)
  {
    bits = CPCMRemap::CountBits(m_pCodecContext->ch_layout.u.mask);
    if (bits == m_pCodecContext->ch_layout.nb_channels)
    {
      return m_pCodecContext->ch_layout.u.mask;
    }
  }

  if(!tryDefault)
  {
    CLogLog(LOGINFO, "COMXAudioCodecOMX::GetChannelMap - bad channel layout order");
    return 0;
  }

  if(bits == -1)
  {
    CLogLog(LOGINFO, "COMXAudioCodecOMX::GetChannelMap - FFmpeg reported invalid channel "
      "layout order ignoring");
  }
  else
  {
    CLogLog(LOGINFO, "COMXAudioCodecOMX::GetChannelMap - FFmpeg reported %d channels, but the "
      "layout contains %d ignoring", m_pCodecContext->ch_layout.nb_channels, bits);
  }

  av_channel_layout_default(&m_pCodecContext->ch_layout, m_pCodecContext->ch_layout.nb_channels);

  tryDefault = false;
  goto retry;
}
#endif
