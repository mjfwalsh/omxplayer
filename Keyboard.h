#pragma once
/*
 *
 *      Copyright (C) 2022 Michael Walsh
 *      Copyright (C) 2012 Edgar Hucek
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
#include "KeyConfig.h"
#include <unordered_map>
#include <atomic>
#include <termios.h>

class Keyboard : public OMXThread
{
 protected:
  struct termios orig_termios;
  int orig_fl;
  std::atomic<int> m_action{-1};

  std::unordered_map<int,int> m_keymap;
 public:
  explicit Keyboard(const char *filename);
  ~Keyboard() override;
  void Process() override;
  void Sleep(unsigned int dwMilliSeconds);
  enum Action getEvent();
};
