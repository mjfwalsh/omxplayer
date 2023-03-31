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
#include "utils/Mailbox.h"
#include "SubtitleRenderer.h"

#include <boost/circular_buffer.hpp>
#include <string>
#include <vector>

class OMXClock;
class OMXPacket;
class SubtitleRenderer;
class Subtitle;
class Rect;
class Dimension;
struct AVCodecContext;

class OMXSubConfig
{
public:
  bool              centered            = false;
  bool              ghost_box           = true;
  unsigned int      subtitle_lines      = 3;
  float             font_size           = 0.055f;
  const char        *reg_font           = "/usr/share/fonts/truetype/freefont/FreeSans.ttf";
  const char        *italic_font        = "/usr/share/fonts/truetype/freefont/FreeSansOblique.ttf";
  const char        *bold_font          = "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf";
};

class OMXPlayerSubtitles : public OMXThread
{
public:
  OMXPlayerSubtitles(const OMXPlayerSubtitles&) = delete;
  OMXPlayerSubtitles& operator=(const OMXPlayerSubtitles&) = delete;

  ~OMXPlayerSubtitles();

  OMXPlayerSubtitles(OMXSubConfig *config,
                     OMXClock* clock);

  bool Open(size_t stream_count, std::string &external_subtitle_path);

  bool initDVDSubs(Rect &view_port, Dimension &sub_dim, uint32_t *palette);

  void Close();
  void Flush();
  void Resume();
  void Pause();

  void SetVisible(bool visible);

  bool GetVisible()
  {
    return m_visible;
  }
  
  int SetActiveStream(int index);

  int SetActiveStreamDelta(int index);

  int GetActiveStream()
  {
    return m_visible ? m_active_index : -1;
  }

  int GetStreamCount()
  {
    return m_stream_count;
  }

  void SetDelay(int value);

  int GetDelay()
  {
    return m_delay;
  }

  void DisplayText(const char *text, int duration, bool wait = false);

  void AddPacket(OMXPacket *pkt);

protected:
  AVCodecContext           *m_dvd_codec_context = NULL;

private:
  void SendToRenderer(Mailbox::Item *msg);
  void SendToRenderer(Mailbox::Type t);
  void Process() override;
  void RenderLoop();
  bool GetSubData(OMXPacket *pkt, Subtitle &sub);
  bool GetTextLines(OMXPacket *pkt, Subtitle &sub);
  bool GetImageData(OMXPacket *pkt, Subtitle &sub);

  std::vector<boost::circular_buffer<Subtitle>> m_subtitle_buffers;
  Mailbox                                       m_mailbox;
  bool                                          m_visible = false;
  int                                           m_external_subtitle_stream = -1;
  std::vector<Subtitle>                         m_external_subtitles;
  int                                           m_active_index = 0;
  int                                           m_stream_count = 0;
  int                                           m_delay = 0;
  SubtitleRenderer                              m_renderer;
  OMXClock*                                     m_av_clock;
  uint32_t                                      *m_palette = NULL;
  int                                           m_prev_pts = -1;
};
