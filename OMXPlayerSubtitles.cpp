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

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "OMXReader.h"
#include "OMXClock.h"
#include "OMXPlayerSubtitles.h"
#include "SubtitleRenderer.h"
#include "Subtitle.h"
#include "utils/log.h"
#include "Srt.h"

#include <signal.h>
#include <stdint.h>

using namespace std;
using namespace boost;

OMXPlayerSubtitles::~OMXPlayerSubtitles()
{
  if(Running())
  {
    SendToRenderer(new Mailbox::Exit());
    StopThread();
  }

  if(m_dvd_codec_context)
    avcodec_free_context(&m_dvd_codec_context);

  if(m_palette)
    delete[] m_palette;

  if(m_renderer)
    delete m_renderer;
}

OMXPlayerSubtitles::OMXPlayerSubtitles(int display,
                                       int layer,
                                       float font_size,
                                       bool centered,
                                       bool ghost_box,
                                       unsigned int lines,
                                       OMXClock* clock)
:
m_av_clock(clock)
{
  m_renderer = new SubtitleRenderer(display, layer,
                                    font_size,
                                    centered,
                                    ghost_box,
                                    lines);

  Create();
}


bool OMXPlayerSubtitles::Open(size_t internal_stream_count, string &subtitle_path)
{
  m_stream_count = internal_stream_count;

  if(subtitle_path.size() > 0)
  {
    if(!ReadSrt(subtitle_path, m_external_subtitles))
      return false;

    m_external_subtitle_stream = m_stream_count;
    m_stream_count++;
  }

  m_subtitle_buffers.resize(internal_stream_count, circular_buffer<Subtitle>(32));

  return true;
}

bool OMXPlayerSubtitles::initDVDSubs(Dimension video, float video_aspect,
		int aspect_mode, uint32_t *palette)
{
  if(palette) {
    if(!m_palette)
      m_palette = new uint32_t[16];

    for(int i = 0; i < 16; i++)
      m_palette[i] = palette[i];
  } else if(m_palette) {
    delete[] m_palette;
    m_palette = NULL;
  }

  SendToRenderer(new Mailbox::DVDSubs(video, video_aspect, aspect_mode, m_palette));

  AVCodec *dvd_codec = avcodec_find_decoder(AV_CODEC_ID_DVD_SUBTITLE);
  if(!dvd_codec)
    return false;
  
  m_dvd_codec_context = avcodec_alloc_context3(dvd_codec);
  if(!m_dvd_codec_context)
    return false;

  if(m_palette == NULL) {
    if(avcodec_open2(m_dvd_codec_context, dvd_codec, NULL) < 0)
      return false;
  } else {
    AVDictionary *d = NULL;
    if(av_dict_set(&d, "palette", "0,1,2,3,4,5,6,7,8,9,A,B,C,D,E,F", 0) < 0)
      return false;

    if(avcodec_open2(m_dvd_codec_context, dvd_codec, &d) < 0)
      return false;

    if(av_dict_count(d) != 0)
      return false;
  }

  return true;
}

void OMXPlayerSubtitles::deInitDVDSubs()
{
  SendToRenderer(new Mailbox::Item(Mailbox::REMOVE_DVD_SUBS));

  if(m_dvd_codec_context)
    avcodec_free_context(&m_dvd_codec_context);

  if(m_palette)
    delete[] m_palette;
}

void OMXPlayerSubtitles::Close()
{
  m_mailbox.clear();
  m_subtitle_buffers.clear();
  m_stream_count = 0;
  m_external_subtitle_stream = -1;
}

void OMXPlayerSubtitles::Process()
{
  try
  {
    RenderLoop();
  }
  catch(const char *e)
  {
    // prints error message
    puts(e);

    // and send quit signal to main thread
    pthread_kill(OMXThread::main_thread, SIGUSR1);
  }
  m_thread_stopped.store(true, memory_order_relaxed);
  m_mailbox.clear();
}

void OMXPlayerSubtitles::SendToRenderer(Mailbox::Item *msg)
{
  if(m_thread_stopped.load(std::memory_order_relaxed))
  {
    CLogLog(LOGERROR, "Subtitle rendering thread not running, message discarded");
    delete msg;
    return;
  }
  if(msg != NULL)
    m_mailbox.send(msg);
}

template <typename Iterator>
Iterator FindSubtitle(Iterator begin, Iterator end, int time)
{
  return upper_bound(begin, end, time,
    [](int a, const Subtitle& b) { return a < b.stop; });
}

void OMXPlayerSubtitles::RenderLoop()
{
  vector<Subtitle> internal_subtitles;
  vector<Subtitle> *subtitles = &internal_subtitles;

  int prev_now = 0;
  size_t next_index = 0;
  bool paused = false;
  bool have_next = false;
  int current_stop = INT_MIN;
  bool showing = false;
  bool osd = false;
  chrono::time_point<std::chrono::steady_clock> osd_stop;
  int delay = 0;

  auto GetCurrentTime = [&]
  {
    return static_cast<int>(m_av_clock->GetMediaTime()/1000) - delay;
  };

  auto TryPrepare = [&](int time)
  {
    for(; next_index != subtitles->size(); ++next_index)
    {
      if(subtitles->at(next_index).stop > time)
      {
        m_renderer->prepare(subtitles->at(next_index));
        have_next = true;
        break;
      }
    }
  };

  auto Reset = [&](int time)
  {
    m_renderer->unprepare();
    current_stop = INT_MIN;

    auto it = FindSubtitle(subtitles->begin(),
                           subtitles->end(),
                           time);
    next_index = it - subtitles->begin();

    if(next_index != subtitles->size())
    {
      m_renderer->prepare(subtitles->at(next_index));
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
      int now = GetCurrentTime();

      int till_stop =
        showing ? current_stop - now
                : INT_MAX;

      int till_next_start =
        have_next ? subtitles->at(next_index).start - now
                  : INT_MAX;

      timeout = min(min(till_stop, till_next_start), 1000);
    }

    if(osd)
    {
      int cap = chrono::duration_cast<std::chrono::milliseconds>(osd_stop - chrono::steady_clock::now()).count();
      if (cap < timeout) timeout = cap;
    }

    // wait for next message or to timeout
    m_mailbox.wait(chrono::milliseconds(timeout));


    while(1)
    {
      Mailbox::Item *args = m_mailbox.receive();
      if(args == NULL)
        break;

      switch(args->type) {
        case Mailbox::ADD_DVD_SUBS:
          {
            Mailbox::DVDSubs *a = (Mailbox::DVDSubs *)args;

            m_renderer->initDVDSubs(
              a->video,
              a->video_aspect,
              a->aspect_mode,
              a->palette
            );
          }
          break;
        case Mailbox::REMOVE_DVD_SUBS:
          m_renderer->deInitDVDSubs();
          break;
        case Mailbox::PUSH:
          {
            Mailbox::Push *a = (Mailbox::Push *)args;
            internal_subtitles.push_back(std::move(a->subtitle));
          }
          break;
        case Mailbox::SEND_INTERNAL_SUBS:
          {
            Mailbox::SendInternalSubs *a = (Mailbox::SendInternalSubs *)args;
            internal_subtitles.swap(a->subtitles);
          }
          prev_now = INT_MAX;
          break;
        case Mailbox::FLUSH:
          internal_subtitles.clear();
          prev_now = INT_MAX;
          break;
        case Mailbox::TOGGLE_EXTERNAL_SUBS:
          {
            Mailbox::ToggleExternalSubs *a = (Mailbox::ToggleExternalSubs *)args;
            subtitles = a->visible ? &m_external_subtitles : &internal_subtitles;
            internal_subtitles.clear();
            prev_now = INT_MAX;
          }
          break;
        case Mailbox::SET_PAUSED:
          {
            Mailbox::SetPaused *a = (Mailbox::SetPaused *)args;
            paused = a->value;
          }
          break;
        case Mailbox::SET_DELAY:
          {
            Mailbox::SetDelay *a = (Mailbox::SetDelay *)args;
            delay = a->value;
            prev_now = INT_MAX;
          }
          break;
        case Mailbox::DISPLAY_TEXT:
          {
            Mailbox::DisplayText *a = (Mailbox::DisplayText *)args;

            m_renderer->prepare(a->text_lines);
            m_renderer->show_next();
            showing = true;
            osd = true;
            osd_stop = chrono::steady_clock::now() +
                       chrono::milliseconds(a->duration);
            prev_now = INT_MAX;
          }
          break;
        case Mailbox::CLEAR_RENDERER:
          m_renderer->clear();
          internal_subtitles.clear();
          subtitles = &internal_subtitles;
          prev_now = INT_MAX;
          break;
        case Mailbox::EXIT:
          return;
          break;
      }

      delete args;
    }

    int now = GetCurrentTime();

    if(now < prev_now || (have_next && subtitles->at(next_index).stop <= now))
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
      if(have_next && subtitles->at(next_index).start <= now)
      {
        m_renderer->show_next();
        // printf("show error: %i ms\n", now - subtitles[next_index].start);
        showing = true;
        current_stop = subtitles->at(next_index).stop;

        ++next_index;
        have_next = false;
        TryPrepare(now);
      }
      else if(showing)
      {
        m_renderer->hide();
        // printf("hide error: %i ms\n", now - current_stop);
        showing = false;
      }
    }
  }
}

void OMXPlayerSubtitles::Flush()
{
  for(auto &q : m_subtitle_buffers)
    q.clear();

  SendToRenderer(new Mailbox::Flush());
}

void OMXPlayerSubtitles::Resume()
{
  SendToRenderer(new Mailbox::SetPaused(false));
}

void OMXPlayerSubtitles::Pause()
{
  SendToRenderer(new Mailbox::SetPaused(true));
}

void OMXPlayerSubtitles::SetDelay(int value)
{
  m_delay = value;
  SendToRenderer(new Mailbox::SetDelay(value));
}

void OMXPlayerSubtitles::Clear()
{
  SendToRenderer(new Mailbox::ClearRenderer());
}

void OMXPlayerSubtitles::SetVisible(bool visible)
{
  if(m_stream_count == 0 || visible == m_visible)
    return;

  SetActiveStream(visible ? m_active_index : -1);
}


int OMXPlayerSubtitles::SetActiveStreamDelta(int delta)
{
  int index;

  if(m_visible)
    index = m_active_index + delta;
  else if(delta == 1)
    index = 0;
  else
    index = m_stream_count - 1;

  return SetActiveStream(index);
}

int OMXPlayerSubtitles::SetActiveStream(int new_index)
{
  // disable whatever subs are there
  if(new_index < 0 || new_index >= m_stream_count) {
    if(m_active_index == m_external_subtitle_stream)
      SendToRenderer(new Mailbox::ToggleExternalSubs(false));
    else
      SendToRenderer(new Mailbox::Flush());

    m_visible = false;
    return -1;
  }

  // nothing to do here
  if(m_visible && new_index == m_active_index)
    return m_active_index;

  // enable external subs and disable internal subs
  if(new_index == m_external_subtitle_stream)
  {
    SendToRenderer(new Mailbox::ToggleExternalSubs(true));
    m_active_index = m_external_subtitle_stream;
    m_visible = true;
    return m_active_index;
  }

  // disable external subs
  if(m_active_index == m_external_subtitle_stream)
  {
    SendToRenderer(new Mailbox::ToggleExternalSubs(false));
  }

  // set new index and set visible to true
  m_active_index = new_index;
  m_visible = true;

  // Send internal subtitle buffer to renderer
  Mailbox::SendInternalSubs *subs = new Mailbox::SendInternalSubs;
  for(auto& s : m_subtitle_buffers[m_active_index])
    subs->subtitles.push_back(s);
  SendToRenderer(subs);

  return m_active_index;
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
  sub.text.resize(pkt->size);

  // replace literal '\N' with newlines, ignore form feeds
  char *r = start;
  int w = 0;

  while (r < end - 1)
  {
    if(*r == '\\' && *(r + 1) == 'N') {
      sub.text[w++] = '\n';
      r += 2;
    } else if(*r == '\r') {
      r++;
    } else {
      sub.text[w++] = *(r++);
    }
  }

  // last char
  if(*r != '\r' && *r != '\n')
    sub.text[w++] = *r;

  sub.text[w++] = '\0';

  return true;
}

bool OMXPlayerSubtitles::GetImageData(OMXPacket *pkt, Subtitle &sub)
{
  AVSubtitle s;
  int got_sub_ptr = -1;
  int r = avcodec_decode_subtitle2(m_dvd_codec_context, &s, &got_sub_ptr, pkt);

  if(r < 0 || got_sub_ptr < 1)
    return false;

  if(s.num_rects < 1 || s.rects[0]->nb_colors != 4)
  {
    avsubtitle_free(&s);
    return false;
  }

  // Fix time
  sub.stop = sub.start + (s.end_display_time - s.start_display_time);

  // calculate palette
  unsigned char palette[4];
  if(m_palette)
  {
    uint32_t *p = (uint32_t *)s.rects[0]->data[1];
    for(int i = 0; i < 4; i++) {
      // merge the most significant four bits (alpha channel) and
      // the least significant four bits (index from the above dummy palette)
      palette[i] = (*p >> 24 & 0xf0) | (*p & 0xf);
      p++;
    }
  }

  // assign data
  sub.assign_image(s.rects[0]->data[0], s.rects[0]->linesize[0] * s.rects[0]->h,
                   m_palette ? &palette[0] : NULL);
  sub.image.rect = {s.rects[0]->x, s.rects[0]->y, s.rects[0]->w, s.rects[0]->h};

  avsubtitle_free(&s);
  return true;
}

void OMXPlayerSubtitles::AddPacket(OMXPacket *pkt)
{
  if(pkt->index >= (int)m_subtitle_buffers.size())
  {
    delete pkt;
    return;
  }

  if(pkt->hints.codec != AV_CODEC_ID_SUBRIP && 
     pkt->hints.codec != AV_CODEC_ID_SSA &&
     pkt->hints.codec != AV_CODEC_ID_ASS &&
     pkt->hints.codec != AV_CODEC_ID_DVD_SUBTITLE)
  {
    delete pkt;
    return;
  }

  Subtitle sub(pkt->hints.codec == AV_CODEC_ID_DVD_SUBTITLE);

  sub.start = static_cast<int>(pkt->pts/1000);
  sub.stop = sub.start + static_cast<int>(pkt->duration/1000);

  if (!m_subtitle_buffers[pkt->index].empty() &&
    sub.stop < m_subtitle_buffers[pkt->index].back().stop)
  {
    sub.stop = m_subtitle_buffers[pkt->index].back().stop;
  }

  if(!(sub.isImage ? GetImageData(pkt, sub) : GetTextLines(pkt, sub)))
  {
    delete pkt;
    return;
  }

  m_subtitle_buffers[pkt->index].push_back(sub);

  if(m_visible && pkt->index == m_active_index)
  {
    SendToRenderer(new Mailbox::Push(sub));
  }

  delete pkt;
}

void OMXPlayerSubtitles::DisplayText(const char *text, int duration)
{
  SendToRenderer(new Mailbox::DisplayText(text, duration));
}
