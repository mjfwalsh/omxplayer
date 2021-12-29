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
#include "OMXReader.h"
#include "OMXClock.h"
#include "OMXPlayerSubtitles.h"
#include "SubtitleRenderer.h"
#include "Subtitle.h"
#include "DllAvCodec.h"
#include "utils/Enforce.h"
#include "utils/ScopeExit.h"
#include "utils/log.h"

#include <boost/algorithm/string.hpp>
#include <utility>
#include <algorithm>
#include <typeinfo>
#include <cstdint>

using namespace std;
using namespace boost;

OMXPlayerSubtitles::OMXPlayerSubtitles() BOOST_NOEXCEPT
: m_visible(),
  m_use_external_subtitles(),
  m_active_index(),
  m_delay(),
  m_thread_stopped(),
  m_font_size(),
  m_centered(),
  m_ghost_box(),
  m_lines(),
  m_av_clock()
{}

OMXPlayerSubtitles::~OMXPlayerSubtitles() BOOST_NOEXCEPT
{
  Close();
}

bool OMXPlayerSubtitles::Init(int display,
                              int layer,
                              float font_size,
                              bool centered,
                              bool ghost_box,
                              unsigned int lines,
                              OMXClock* clock) BOOST_NOEXCEPT
{
  m_visible = true;
  m_use_external_subtitles = false;
  m_active_index = 0;
  m_delay = 0;
  m_thread_stopped.store(false, memory_order_relaxed);

  m_font_size = font_size;
  m_centered = centered;
  m_ghost_box = ghost_box;
  m_lines = lines;
  m_av_clock = clock;
  m_display = display;
  m_layer = layer;

  if(!Create())
    return false;

  return true;
}


bool OMXPlayerSubtitles::Open(size_t stream_count,
                              vector<Subtitle>&& external_subtitles) BOOST_NOEXCEPT
{
  if(external_subtitles.size() > 0)
  {
    m_subtitle_buffers.clear();

    SendToRenderer(Message::SendExternalSubs{std::move(external_subtitles)});
    m_use_external_subtitles = true;
    m_stream_count = 1;
  }
  else
  {
    m_subtitle_buffers.resize(stream_count, circular_buffer<Subtitle>(32));
    m_stream_count = stream_count;
  }

  return true;
}

bool OMXPlayerSubtitles::initDVDSubs(Dimension video,
                              float video_aspect,
                              int aspect_mode) BOOST_NOEXCEPT
{
  SendToRenderer(Message::DVDSubs{video, video_aspect, aspect_mode});

  AVCodec *dvd_codec = avcodec_find_decoder(AV_CODEC_ID_DVD_SUBTITLE);
  m_dvd_codec_context = avcodec_alloc_context3(dvd_codec);
  avcodec_open2(m_dvd_codec_context, dvd_codec, NULL);

  return true;
}

void OMXPlayerSubtitles::Close() BOOST_NOEXCEPT
{
  m_mailbox.clear();
  m_subtitle_buffers.clear();
  m_stream_count = 0;
}

void OMXPlayerSubtitles::DeInit() BOOST_NOEXCEPT
{
  if(Running())
  {
    SendToRenderer(Message::Stop{});
    StopThread();
  }

  if(m_dvd_codec_context)
    avcodec_free_context(&m_dvd_codec_context);
}

void OMXPlayerSubtitles::Process()
{
  try
  {
    RenderLoop(m_font_size, m_centered,
               m_ghost_box, m_lines,
               m_av_clock);
  }
  catch(Enforce_error& e)
  {
    if(!e.user_friendly_what().empty())
      printf("Error: %s\n", e.user_friendly_what().c_str());
    CLogLog(LOGERROR, "OMXPlayerSubtitles::RenderLoop threw %s (%s)",
              typeid(e).name(), e.what());
  }
  catch(std::exception& e)
  {
    CLogLog(LOGERROR, "OMXPlayerSubtitles::RenderLoop threw %s (%s)",
              typeid(e).name(), e.what());
  }
  m_thread_stopped.store(true, memory_order_relaxed);
}

template <typename Iterator>
Iterator FindSubtitle(Iterator begin, Iterator end, int time)
{
  return upper_bound(begin, end, time,
    [](int a, const Subtitle& b) { return a < b.stop; });
}

void OMXPlayerSubtitles::
RenderLoop(float font_size,
           bool centered,
           bool ghost_box,
           unsigned int lines,
           OMXClock* clock)
{
  SubtitleRenderer renderer(m_display, m_layer,
                            font_size,
                            centered,
                            ghost_box,
                            lines);

  vector<Subtitle> external_subtitles;
  vector<Subtitle> subtitles;

  bool external_subtitles_enabled = false;

  int prev_now{};
  size_t next_index{};
  bool exit{};
  bool paused{};
  bool have_next{};
  int current_stop = INT_MIN;
  bool showing{};
  bool osd{};
  chrono::time_point<std::chrono::steady_clock> osd_stop;
  int delay{};

  auto GetCurrentTime = [&]
  {
    return static_cast<int>(clock->OMXMediaTime()/1000) - delay;
  };

  auto TryPrepare = [&](int time)
  {
    for(; next_index != subtitles.size(); ++next_index)
    {
      if(subtitles[next_index].stop > time)
      {
        renderer.prepare(subtitles[next_index]);
        have_next = true;
        break;
      }
    }
  };

  auto Reset = [&](int time)
  {
    renderer.unprepare();
    current_stop = INT_MIN;

    auto it = FindSubtitle(subtitles.begin(),
                           subtitles.end(),
                           time);
    next_index = it - subtitles.begin();

    if(next_index != subtitles.size())
    {
      renderer.prepare(subtitles[next_index]);
      have_next = true;
    }
    else
    {
      have_next = false;
    }
  };

  for(;;)
  {
    int timeout = INT_MAX;

    if(!paused)
    {
      auto now = GetCurrentTime();

      int till_stop =
        showing ? current_stop - now
                : INT_MAX;

      int till_next_start =
        have_next ? subtitles[next_index].start - now
                  : INT_MAX;

      timeout = min(min(till_stop, till_next_start), 1000);
    }

    if(osd)
    {
      int cap = chrono::duration_cast<std::chrono::milliseconds>(osd_stop - chrono::steady_clock::now()).count();
      if (cap < timeout) timeout = cap;
    }

    m_mailbox.receive_wait(chrono::milliseconds(timeout),
      [&](Message::DVDSubs&& args)
      {
        renderer.initDVDSubs(
          args.video,
          args.video_aspect,
          args.aspect_mode
        );
      },
      [&](Message::Push&& args) // Add internal subs from muxer
      {
        subtitles.push_back(std::move(args.subtitle));
      },
      [&](Message::SendExternalSubs&& args)
      {
        external_subtitles = std::move(args.subtitles);
        subtitles.swap(external_subtitles);
        external_subtitles_enabled = true;
        prev_now = INT_MAX;
      },
      [&](Message::ToggleExternalSubs&& args)
      {
        if(external_subtitles_enabled != args.enable_subs)
        {
          subtitles.swap(external_subtitles);
          external_subtitles_enabled = args.enable_subs;
        }
        prev_now = INT_MAX;
      },
      [&](Message::Flush&& args) // Sets or clears internal subs
      {
        subtitles = std::move(args.subtitles);
        prev_now = INT_MAX;
      },
      [&](Message::Touch&&) // External subs
      {
      },
      [&](Message::SetPaused&& args)
      {
        paused = args.value;
      },
      [&](Message::SetDelay&& args)
      {
        delay = args.value;
        prev_now = INT_MAX;
      },
      [&](Message::Stop&&)
      {
        exit = true;
      },
      [&](Message::DisplayText&& args) // display osd
      {
        renderer.prepare(args.text_lines);
        renderer.show_next();
        showing = true;
        osd = true;
        osd_stop = chrono::steady_clock::now() +
                   chrono::milliseconds(args.duration);
        prev_now = INT_MAX;
      },
      [&](Message::Clear&&)
      {
        renderer.clear();
      });

    if(exit) break;

    auto now = GetCurrentTime();

    if(now < prev_now || (have_next && subtitles[next_index].stop <= now))
    {
      Reset(now);
    }
    else if(!have_next)
    {
      TryPrepare(now);
    }

    prev_now = now;

    if(osd && chrono::steady_clock::now() >= osd_stop)
      osd = false;

    if(!osd && current_stop <= now)
    {
      if(have_next && subtitles[next_index].start <= now)
      {
        renderer.show_next();
        // printf("show error: %i ms\n", now - subtitles[next_index].start);
        showing = true;
        current_stop = subtitles[next_index].stop;

        ++next_index;
        have_next = false;
        TryPrepare(now);
      }
      else if(showing)
      {
        renderer.hide();
        // printf("hide error: %i ms\n", now - current_stop);
        showing = false;
      }
    }
  }
}

void OMXPlayerSubtitles::FlushRenderer()
{
  assert(m_visible);

  if(m_use_external_subtitles)
  {
    SendToRenderer(Message::ToggleExternalSubs{true});
  }
  else
  {
    Message::Flush flush;
    assert(!m_subtitle_buffers.empty());
    for(auto& s : m_subtitle_buffers[m_active_index])
      flush.subtitles.push_back(s);
    SendToRenderer(std::move(flush));
  }
}

void OMXPlayerSubtitles::Flush() BOOST_NOEXCEPT
{
  for(auto& q : m_subtitle_buffers)
    q.clear();

  if(m_visible)
  {
    if(m_use_external_subtitles)
      SendToRenderer(Message::Touch{});
    else
      SendToRenderer(Message::Flush{});
  }
}

void OMXPlayerSubtitles::Resume() BOOST_NOEXCEPT
{
  SendToRenderer(Message::SetPaused{false});
}

void OMXPlayerSubtitles::Pause() BOOST_NOEXCEPT
{
  SendToRenderer(Message::SetPaused{true});
}

void OMXPlayerSubtitles::SetDelay(int value) BOOST_NOEXCEPT
{
  m_delay = value;
  SendToRenderer(Message::SetDelay{value});
}

void OMXPlayerSubtitles::Clear() BOOST_NOEXCEPT
{
  SendToRenderer(Message::Clear{});
}

void OMXPlayerSubtitles::SetVisible(bool visible) BOOST_NOEXCEPT
{
  if(visible)
  {
    if (!m_visible)
    {
      m_visible = true;
      FlushRenderer();
    }
  }
  else
  {
    if(m_visible)
    {
      m_visible = false;

      if(m_use_external_subtitles)
        SendToRenderer(Message::ToggleExternalSubs{false});
      else
        SendToRenderer(Message::Flush{});
    }
  }
}

void OMXPlayerSubtitles::SetActiveStreamDelta(int delta) BOOST_NOEXCEPT
{
  if(m_visible) {
    SetActiveStream(m_active_index + delta);
  } else if(delta == 1) {
    SetActiveStream(0);
  } else if(delta == -1) {
    SetActiveStream(m_stream_count - 1);
  }
}

void OMXPlayerSubtitles::SetActiveStream(int index) BOOST_NOEXCEPT
{
  if(m_stream_count == 1) {
    SetVisible(index == 0);
  } else if(index < 0 || index >= m_stream_count) {
    SetVisible(false);
  } else {
    SetVisible(true);
    m_active_index = index;
    if(m_visible)
      FlushRenderer();
  }
}

bool OMXPlayerSubtitles::GetTextLines(OMXPacket *pkt, Subtitle &sub)
{
  char *start, *end;
  start = (char*)pkt->data;
  end   = (char*)pkt->data + pkt->size;

  // skip the prefixed ssa fields (8 fields)
  if (pkt->hints.codec == AV_CODEC_ID_SSA || pkt->hints.codec == AV_CODEC_ID_ASS)
  {
    int nFieldCount = 8;
    while (nFieldCount > 0 && start < end)
      if (*(start++) == ',')
        nFieldCount--;
  }

  // allocate space
  sub.alloc_text(pkt->size);

  // replace literal '\N' with newlines, ignore form feeds
  char *r = start;
  char *w = sub.text.lines;

  while (r < end - 1)
  {
    if(*r == '\\' && *(r + 1) == 'N') {
      *(w++) = '\n';
      r += 2;
    } else if(*r == '\r') {
      r++;
    } else {
      *(w++) = *(r++);
    }
  }

  // last char
  if(*r != '\r' && *r != '\n')
    *(w++) = *r;

  *w = '\0';

  sub.text.length = w - sub.text.lines;

  return true;
}

bool OMXPlayerSubtitles::GetImageData(OMXPacket *pkt, Subtitle &sub)
{
  AVSubtitle s;
  int got_sub_ptr = -1;
  avcodec_decode_subtitle2(m_dvd_codec_context, &s, &got_sub_ptr, pkt);

  if(got_sub_ptr < 1 || s.num_rects < 1 || s.rects[0]->nb_colors != 4)
  {
    avsubtitle_free(&s);
    return false;
  }

  // Fix time
  sub.stop = sub.start + (s.end_display_time - s.start_display_time);

  // assign data
  sub.assign_image(s.rects[0]->data[0], s.rects[0]->linesize[0] * s.rects[0]->h);
  sub.image.rect = {s.rects[0]->x, s.rects[0]->y, s.rects[0]->w, s.rects[0]->h};

  avsubtitle_free(&s);
  return true;
}

void OMXPlayerSubtitles::AddPacket(OMXPacket *pkt, size_t stream_index) BOOST_NOEXCEPT
{
  assert(stream_index < m_subtitle_buffers.size());

  SCOPE_EXIT
  {
    delete pkt;
  };

  if(pkt->hints.codec != AV_CODEC_ID_SUBRIP && 
     pkt->hints.codec != AV_CODEC_ID_SSA &&
     pkt->hints.codec != AV_CODEC_ID_ASS &&
     pkt->hints.codec != AV_CODEC_ID_DVD_SUBTITLE)
  {
    return;
  }

  Subtitle sub(pkt->hints.codec == AV_CODEC_ID_DVD_SUBTITLE);

  sub.start = static_cast<int>(pkt->pts/1000);
  sub.stop = sub.start + static_cast<int>(pkt->duration/1000);

  if (!m_subtitle_buffers[stream_index].empty() &&
    sub.stop < m_subtitle_buffers[stream_index].back().stop)
  {
    sub.stop = m_subtitle_buffers[stream_index].back().stop;
  }

  bool success;
  if(pkt->hints.codec == AV_CODEC_ID_DVD_SUBTITLE)
    success = GetImageData(pkt, sub);
  else
    success = GetTextLines(pkt, sub);
  if(!success) return;

  m_subtitle_buffers[stream_index].push_back(sub);

  if(!m_use_external_subtitles &&
     m_visible &&
     stream_index == GetActiveStream())
  {
    SendToRenderer(Message::Push{std::move(sub)});
  }
}

void OMXPlayerSubtitles::DisplayText(const char *text, int duration) BOOST_NOEXCEPT
{
  SendToRenderer(Message::DisplayText{text, duration});
}
