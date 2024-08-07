#pragma once
/*
 *
 *      Copyright (C) 2022 Michael Walsh
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

#include <atomic>

enum Action;

class CECListener
{
protected:
  std::atomic<int> m_action{-1};

public:
  CECListener();
  enum Action getEvent();

private:
  static void InitCallback(void *object, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4);
  static void ActionCallback(void *object, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4);
};
