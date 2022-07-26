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

enum ControlFlow {
  CONTINUE = -1,
//EXIT_SUCCESS = 0,
//EXIT_FAILURE = 2,
  PLAY_STOPPED = 3,
  CHANGE_FILE = 4,
  CHANGE_PLAYLIST_ITEM = 5,
  RUN_PLAY_LOOP = 6,
  END_PLAY = 7,
  END_PLAY_WITH_ERROR = 8,
  ABORT_PLAY = 9,
  SHUTDOWN = 10,
};

class DMessage;

enum ControlFlow handle_event(enum Action search_key, DMessage *m);
