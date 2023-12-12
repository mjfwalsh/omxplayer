/*
 * Original Author: Torarin Hals Bakke (2012)
 * With changes by Michael Walsh (2022)
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
#include "utils/defs.h"
#include "utils/log.h"
#include "Srt.h"
#include "DispmanxLayer.h"

#include <signal.h>
#include <stdint.h>

using namespace std;
using namespace boost;

OMXPlayerSubtitles::~OMXPlayerSubtitles()
{
  if(Running())
  {
    SendToRenderer(Mailbox::EXIT);
    StopThread();
  }

  if(m_dvd_codec_context)
    avcodec_free_context(&m_dvd_codec_context);

  if(m_palette)
    delete[] m_palette;
}

OMXPlayerSubtitles::OMXPlayerSubtitles(OMXSubConfig *config,
                                       OMXClock* clock)
:
m_renderer(config),
m_av_clock(clock)
{
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

bool OMXPlayerSubtitles::initDVDSubs(Rect &view_port, Dimension &sub_dim, uint32_t *palette)
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

  AVCONST AVCodec *dvd_codec = avcodec_find_decoder(AV_CODEC_ID_DVD_SUBTITLE);
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

  DispmanxLayer *layer = new DispmanxLayer(1, view_port, sub_dim, palette);
  SendToRenderer(new Mailbox::DVDSubs(layer));

  return true;
}

void OMXPlayerSubtitles::Close()
{
  sem_t close_wait;
  sem_init(&close_wait, 0, 0);

  SendToRenderer(new Mailbox::Close(&close_wait));

  // we use this semaphore to ensure the thread has finished with
  // m_external_subtitles, m_dvd_codec_context and other variables
  sem_wait(&close_wait);
  sem_destroy(&close_wait);

  m_external_subtitles.clear();
  m_subtitle_buffers.clear();
  
  m_stream_count = 0;
  m_external_subtitle_stream = -1;

  if(m_dvd_codec_context)
    avcodec_free_context(&m_dvd_codec_context);

  if(m_palette)
  {
    delete[] m_palette;
    m_palette = NULL;
  }
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
  m_mailbox.finish();
}

void OMXPlayerSubtitles::SendToRenderer(Mailbox::Item *msg)
{
  m_mailbox.send(msg);
}

void OMXPlayerSubtitles::SendToRenderer(Mailbox::Type msg)
{
  m_mailbox.send(new Mailbox::Item(msg));
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
  bool wait_for_osd = false;
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
        m_renderer.prepare(subtitles->at(next_index));
        have_next = true;
        break;
      }
    }
  };

  auto Reset = [&](int time)
  {
    m_renderer.unprepare();
    current_stop = INT_MIN;

    auto it = FindSubtitle(subtitles->begin(),
                           subtitles->end(),
                           time);
    next_index = it - subtitles->begin();

    if(next_index != subtitles->size())
    {
      m_renderer.prepare(subtitles->at(next_index));
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

            m_renderer.setDVDSubtitleLayer(a->layer);
          }
          break;
        case Mailbox::PUSH:
          {
            Mailbox::Push *a = (Mailbox::Push *)args;

            OMXPacket *pkt = a->pkt;

            Subtitle sub;
            if(GetSubData(pkt, sub))
            {
              // Add to buffer
              m_subtitle_buffers[pkt->stream_type_index].push_back(sub);

              // and send to renderer if this subtitle stream is currently showing
              if(a->currently_showing)
                internal_subtitles.push_back(sub);
            }

            delete pkt;
          }
          break;
        case Mailbox::USE_INTERNAL_SUBS:
          {
            Mailbox::UseInternalSubs *a = (Mailbox::UseInternalSubs *)args;

            internal_subtitles.clear();
            m_prev_pts = -1;
            subtitles = &internal_subtitles;
            for(Subtitle &s : m_subtitle_buffers[a->active_stream])
              internal_subtitles.push_back(s);
          }
          prev_now = INT_MAX;
          break;
        case Mailbox::FLUSH:
          internal_subtitles.clear();
          for(auto &q : m_subtitle_buffers)
            q.clear();
          prev_now = INT_MAX;
          m_prev_pts = -1;
          break;
        case Mailbox::USE_EXTERNAL_SUBS:
          subtitles = &m_external_subtitles;
          prev_now = INT_MAX;
          break;
        case Mailbox::HIDE_SUBS:
          subtitles = &internal_subtitles;
          internal_subtitles.clear();
          prev_now = INT_MAX;
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

            m_renderer.prepare(a->text_lines);
            m_renderer.show_next();
            showing = true;
            osd = true;
            wait_for_osd = a->wait;
            osd_stop = chrono::steady_clock::now() +
                       chrono::milliseconds(a->duration);
            prev_now = INT_MAX;
          }
          break;
        case Mailbox::CLOSE:
          m_renderer.clear();
          internal_subtitles.clear();
          subtitles = &internal_subtitles;
          prev_now = INT_MAX;
          m_prev_pts = -1;
          break;
        case Mailbox::EXIT:
          delete args;

          // wait for any active osd to finish up
          if(osd && wait_for_osd)
          {
            int cap = chrono::duration_cast<std::chrono::milliseconds>(
                                    osd_stop - chrono::steady_clock::now()).count();
            if (cap > 0) OMXClock::Sleep(cap);
          }
          return;
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
        m_renderer.show_next();
        // printf("show error: %i ms\n", now - subtitles[next_index].start);
        showing = true;
        current_stop = subtitles->at(next_index).stop;

        ++next_index;
        have_next = false;
        TryPrepare(now);
      }
      else if(showing)
      {
        m_renderer.hide();
        // printf("hide error: %i ms\n", now - current_stop);
        showing = false;
      }
    }
  }
}

void OMXPlayerSubtitles::Flush()
{
  SendToRenderer(Mailbox::FLUSH);
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

void OMXPlayerSubtitles::SetVisible(bool visible)
{
  if(m_stream_count == 0 || visible == m_visible)
    return;

  SetActiveStream(visible ? m_active_index : -1);
}


int OMXPlayerSubtitles::SetActiveStreamDelta(int delta)
{
  int index;

  if(m_stream_count == 0)
    return -1;
  else if(m_visible)
    index = m_active_index + delta;
  else if(delta == 1)
    index = 0;
  else
    index = m_stream_count - 1;

  return SetActiveStream(index);
}

int OMXPlayerSubtitles::SetActiveStream(int new_index)
{
  if(new_index < 0 || new_index >= m_stream_count) new_index = -1;

  // no subs so nothing to do
  if(m_stream_count == 0)
    return -1;

  // this subtitle stream is already active
  else if(m_visible && new_index == m_active_index)
    return m_active_index;

  // subtitles are already hidden
  else if(!m_visible && new_index == -1)
    return -1;

  // user has selected a sub out of range, so hide the subs
  else if(new_index == -1) {
    SendToRenderer(Mailbox::HIDE_SUBS);
    m_visible = false;
    return -1;
  }

  // we're going from internal subs to external subs
  else if(new_index == m_external_subtitle_stream)
  {
    SendToRenderer(Mailbox::USE_EXTERNAL_SUBS);
    m_active_index = m_external_subtitle_stream;
    m_visible = true;
    return m_active_index;
  }

  else
  {
    // set new index and set visible to true
    m_active_index = new_index;
    m_visible = true;

    // Send internal subtitle buffer to renderer
    SendToRenderer(new Mailbox::UseInternalSubs(m_active_index));

    return m_active_index;
  }
}

bool OMXPlayerSubtitles::GetTextLines(OMXPacket *pkt, Subtitle &sub)
{
  char *start, *end;
  start = (char*)pkt->avpkt->data;
  end   = (char*)pkt->avpkt->data + pkt->avpkt->size;

  // set time
  sub.start = static_cast<int>(pkt->avpkt->pts/1000);
  sub.stop = sub.start + static_cast<int>(pkt->avpkt->duration/1000);

  // skip the prefixed ssa fields (8 fields)
  if (pkt->hints.codec == AV_CODEC_ID_SSA || pkt->hints.codec == AV_CODEC_ID_ASS)
  {
    int nFieldCount = 8;
    while (nFieldCount > 0 && start < end)
      if (*(start++) == ',')
        nFieldCount--;
  }

  // allocate space
  sub.text.resize(pkt->avpkt->size);

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
  int got_sub_ptr;

  if(avcodec_decode_subtitle2(m_dvd_codec_context, &s, &got_sub_ptr, pkt->avpkt) < 0)
    return false;

  // partial packet with timestamp but no content
  if(got_sub_ptr == 0)
  {
    if(pkt->avpkt->pts != AV_NOPTS_VALUE)
      m_prev_pts = static_cast<int>(pkt->avpkt->pts/1000);
    return false;
  }

  // DVD Subs only have one rectangle
  AVSubtitleRect *r = s.rects[0];

  // At the moment we only support 4 colour palettes
  if(r->nb_colors != 4)
    goto ignore_sub;

  // set time
  if(pkt->avpkt->pts != AV_NOPTS_VALUE)
    sub.start = static_cast<int>(pkt->avpkt->pts/1000);
  else if(m_prev_pts != -1)
    sub.start = m_prev_pts;
  else
    goto ignore_sub;

  sub.stop = sub.start + (s.end_display_time - s.start_display_time);

  // set sub rectangle
  sub.image.rect = {r->x, r->y, r->w, r->h};

  // calculate palette and copy data
  if(m_palette)
    sub.assign_image(r->data[0], r->linesize[0] * r->h, (uint32_t *)r->data[1]);
  else
    sub.assign_image(r->data[0], r->linesize[0] * r->h, NULL);

  // tidy up
  avsubtitle_free(&s);
  m_prev_pts = sub.stop;
  return true;

ignore_sub:
  avsubtitle_free(&s);
  m_prev_pts = -1;
  return false;
}

bool OMXPlayerSubtitles::GetSubData(OMXPacket *pkt, Subtitle &sub)
{
  sub.isImage = pkt->hints.codec == AV_CODEC_ID_DVD_SUBTITLE;

  if(sub.isImage)
  {
    if(!GetImageData(pkt, sub))
      return false;
  }
  else
  {
    if(!GetTextLines(pkt, sub))
      return false;
  }

  return true;
}

void OMXPlayerSubtitles::AddPacket(OMXPacket *pkt)
{
  if(pkt->hints.codec != AV_CODEC_ID_SUBRIP &&
     pkt->hints.codec != AV_CODEC_ID_SSA &&
     pkt->hints.codec != AV_CODEC_ID_ASS &&
     pkt->hints.codec != AV_CODEC_ID_DVD_SUBTITLE)
  {
    delete pkt;
    return;
  }

  SendToRenderer(new Mailbox::Push(pkt, m_visible && pkt->stream_type_index == m_active_index));
}

void OMXPlayerSubtitles::DisplayText(const char *text, int duration, bool wait)
{
  SendToRenderer(new Mailbox::DisplayText(text, duration, wait));
}
