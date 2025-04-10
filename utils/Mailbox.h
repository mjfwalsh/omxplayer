#pragma once

// Author: Michael Walsh (2022)

// Boost Software License - Version 1.0 - August 17th, 2003

// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:

// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#include <mutex>
#include <condition_variable>
#include <semaphore.h>

class DispmanxLayer;
class OMXPacket;

class Mailbox {
public:
  enum Type {
    ADD_DVD_SUBS,
    CLOSE,
    PUSH,
    USE_INTERNAL_SUBS,
    USE_EXTERNAL_SUBS,
    HIDE_SUBS,
    FLUSH,
    SET_PAUSED,
    SET_DELAY,
    DISPLAY_TEXT,
    EXIT,
  };

  class Item {
    public:
    explicit Item(const enum Type &t) : type(t) {};
    virtual ~Item() {};

    enum Type type;
    Item *next = NULL;
  };
  class Close : public Item
  {
    public:
    explicit Close(sem_t *w)
      :
      Item(CLOSE),
      sem(w)
    {};

    ~Close() override
    {
      sem_post(sem);
    };

    sem_t *sem;
  };
  class DVDSubs : public Item
  {
    public:
    explicit DVDSubs(DispmanxLayer *dl) :
      Item(ADD_DVD_SUBS), layer(dl) {};

    DispmanxLayer *layer;
  };
  class Push : public Item
  {
    public:
    Push(OMXPacket *p, bool s) : Item(PUSH), pkt(p), currently_showing(s) {};

    OMXPacket *pkt;
    bool currently_showing;
  };
  class UseInternalSubs : public Item
  {
    public:
    explicit UseInternalSubs(const int &as) : Item(USE_INTERNAL_SUBS), active_stream(as)  {};

    int active_stream;
  };
  class SetPaused : public Item
  {
    public:
    explicit SetPaused(bool v) : Item(SET_PAUSED), value(v) {};

    bool value;
  };
  class SetDelay : public Item
  {
    public:
    explicit SetDelay(bool v) : Item(SET_DELAY), value(v) {};

    int value;
  };
  class DisplayText : public Item
  {
    public:
    DisplayText(const char *tl, int d, bool w)
    : Item(DISPLAY_TEXT),
      text_lines(tl),
      duration(d),
      wait(w)
    {};

    std::string text_lines;
    int duration;
    bool wait;
  };

  void send(Item *elem)
  {
    std::lock_guard<std::mutex> look(messages_lock);

    if(finished) {
      delete elem;
      return;
    }

    if(tail == NULL) {
      head = tail = elem;
    } else {
      tail->next = elem;
      tail = elem;
    }
    messages_cond.notify_one();
  }

  Item *receive()
  {
    std::lock_guard<std::mutex> look(messages_lock);

    if(head == NULL) {
      return NULL;
    } else {
      Item *old_head = head;
      head = head->next;

      if(head == NULL)
        tail = NULL;

      return old_head;
    }
  }

  void wait(const std::chrono::milliseconds &rel_time)
  {
    std::unique_lock<std::mutex> lock(messages_lock);
    messages_cond.wait_for(lock, rel_time, [&]{return head != NULL;});
  }

  void finish()
  {
    std::lock_guard<std::mutex> look(messages_lock);

    while(head != NULL)
    {
      Item *old_head = head;
      head = head->next;
      delete old_head;
    }
    tail = NULL;
    finished = true;
  }

private:
  Item *head = NULL;
  Item *tail = NULL;

  std::mutex messages_lock;
  std::condition_variable messages_cond;
  bool finished = false;
};
