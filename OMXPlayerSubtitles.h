#pragma once

/*
 * Author: Torarin Hals Bakke (2012)
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

#include "OMXThread.h"
#include "Subtitle.h"
#include "utils/Mailbox.h"
#include "utils/log.h"

#include <boost/circular_buffer.hpp>
#include <atomic>
#include <string>
#include <vector>

class OMXClock;
class OMXPacket;
class SubtitleRenderer;

class OMXPlayerSubtitles : public OMXThread
{
public:
  OMXPlayerSubtitles(const OMXPlayerSubtitles&) = delete;
  OMXPlayerSubtitles& operator=(const OMXPlayerSubtitles&) = delete;

  ~OMXPlayerSubtitles();

  OMXPlayerSubtitles(int display,
                     int layer,
                     float font_size,
                     bool centered,
                     bool ghost_box,
                     unsigned int lines,
                     OMXClock* clock);

  bool Open(size_t stream_count, std::string &external_subtitle_path);

  bool initDVDSubs(Dimension video, float video_aspect, int aspect_mode, uint32_t *palette);
  void deInitDVDSubs();

  void Close();
  void Flush();
  void Resume();
  void Pause();
  void Clear();

  void SetVisible(bool visible);

  bool GetVisible()
  {
    return m_visible;
  }
  
  int SetActiveStream(int index);

  int SetActiveStreamDelta(int index);

  int GetActiveStream()
  {
    return m_active_index;
  }

  void SetDelay(int value);

  int GetDelay()
  {
    return m_delay;
  }

  void DisplayText(const char *text, int duration);

  void AddPacket(OMXPacket *pkt);

protected:
  AVCodecContext           *m_dvd_codec_context = NULL;

private:
  void SendToRenderer(Mailbox::Item *msg);
  void Process() override;
  void RenderLoop();
  bool GetTextLines(OMXPacket *pkt, Subtitle &sub);
  bool GetImageData(OMXPacket *pkt, Subtitle &sub);

  std::vector<boost::circular_buffer<Subtitle>> m_subtitle_buffers;
  Mailbox                                       m_mailbox;
  bool                                          m_visible;
  int                                           m_external_subtitle_stream;
  std::vector<Subtitle>                         m_external_subtitles;
  int                                           m_active_index;
  int                                           m_stream_count = 0;
  int                                           m_delay;
  std::atomic<bool>                             m_thread_stopped;
  SubtitleRenderer                              *m_renderer;
  OMXClock*                                     m_av_clock;
  uint32_t                                      *m_palette = NULL;
};
