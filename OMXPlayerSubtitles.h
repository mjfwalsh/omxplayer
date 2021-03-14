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

class OMXPlayerSubtitles : public OMXThread
{
public:
  OMXPlayerSubtitles(const OMXPlayerSubtitles&) = delete;
  OMXPlayerSubtitles& operator=(const OMXPlayerSubtitles&) = delete;
  OMXPlayerSubtitles() BOOST_NOEXCEPT;
  ~OMXPlayerSubtitles() BOOST_NOEXCEPT;

  bool Init(int display,
            int layer,
            float font_size,
            bool centered,
            bool ghost_box,
            unsigned int lines,
            OMXClock* clock) BOOST_NOEXCEPT;

  bool Open(size_t stream_count,
            vector<Subtitle>&& external_subtitles) BOOST_NOEXCEPT;

  bool initDVDSubs(Dimension video,
            float video_aspect,
            int aspect_mode) BOOST_NOEXCEPT;

  void Close() BOOST_NOEXCEPT;
  void DeInit() BOOST_NOEXCEPT;
  void Flush() BOOST_NOEXCEPT;
  void Resume() BOOST_NOEXCEPT;
  void Pause() BOOST_NOEXCEPT;
  void Clear() BOOST_NOEXCEPT;

  void SetVisible(bool visible) BOOST_NOEXCEPT;

  bool GetVisible() BOOST_NOEXCEPT
  {
    return m_visible;
  }
  
  void SetActiveStream(size_t index) BOOST_NOEXCEPT;

  size_t GetActiveStream() BOOST_NOEXCEPT
  {
    assert(!m_subtitle_buffers.empty());
    return m_active_index;
  }

  void SetDelay(int value) BOOST_NOEXCEPT;

  int GetDelay() BOOST_NOEXCEPT
  {
    return m_delay;
  }

  void SetUseExternalSubtitles(bool use) BOOST_NOEXCEPT;

  bool GetUseExternalSubtitles() BOOST_NOEXCEPT
  {
    return m_use_external_subtitles;
  }

  void DisplayText(const char *text, int duration) BOOST_NOEXCEPT;

  void AddPacket(OMXPacket *pkt, size_t stream_index) BOOST_NOEXCEPT;

protected:
  AVCodecContext           *m_dvd_codec_context;

private:
  struct Message {
    struct DVDSubs
    {
      Dimension video;
      float video_aspect;
      int aspect_mode;
    };
    struct Stop {};
    struct Flush
    {
      std::vector<Subtitle> subtitles;
    };
    struct SendExternalSubs
    {
      std::vector<Subtitle> subtitles;
    };
    struct ToggleExternalSubs
    {
      bool enable_subs;
    };
    struct Push
    {
      Subtitle subtitle;
    };
    struct Touch {};
    struct SetDelay
    {
      int value;
    };
    struct SetPaused
    {
      bool value;
    };
    struct DisplayText
    {
      std::string text_lines;
      int duration;
    };
    struct Clear {};
  };

  template <typename T>
  void SendToRenderer(T&& msg)
  {
    if(m_thread_stopped.load(std::memory_order_relaxed))
    {
      CLogLog(LOGERROR, "Subtitle rendering thread not running, message discarded");
      return;
    }
    m_mailbox.send(std::forward<T>(msg));
  }

  void Process();
  void RenderLoop(float font_size,
                  bool centered,
                  bool ghost_box,
                  unsigned int lines,
                  OMXClock* clock);
  bool GetTextLines(OMXPacket *pkt, Subtitle &sub);
  bool GetImageData(OMXPacket *pkt, Subtitle &sub);
  void FlushRenderer();

  std::vector<boost::circular_buffer<Subtitle>> m_subtitle_buffers;
  Mailbox<Message::DVDSubs,
          Message::Stop,
          Message::SendExternalSubs,
          Message::ToggleExternalSubs,
          Message::Flush,
          Message::Push,
          Message::Touch,
          Message::SetPaused,
          Message::SetDelay,
          Message::DisplayText,
          Message::Clear>                       m_mailbox;
  bool                                          m_visible;
  bool                                          m_use_external_subtitles;
  size_t                                        m_active_index;
  int                                           m_delay;
  std::atomic<bool>                             m_thread_stopped;
  float                                         m_font_size;
  bool                                          m_centered;
  bool                                          m_ghost_box;
  unsigned int                                  m_lines;
  OMXClock*                                     m_av_clock;
  int                                           m_display;
  int                                           m_layer;
};
