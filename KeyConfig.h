#pragma once
/*
 * Copyright (C) 2022 by Michael J. Walsh
 * Copyright (C) 2013 by Mitch Draves
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdnav; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <unordered_map>

enum Action
{
  INVALID_ACTION = -1,

  // These enums are used by the Action dbus method so the
  // numbers should be maintained for compatibility purposes
  ACTION_DECREASE_SPEED = 1,
  ACTION_INCREASE_SPEED = 2,
  //ACTION_REWIND = 3,
  //ACTION_FAST_FORWARD = 4,
  //ACTION_SHOW_INFO = 5,
  ACTION_PREVIOUS_AUDIO = 6,
  ACTION_NEXT_AUDIO = 7,
  ACTION_PREVIOUS_CHAPTER = 8,
  ACTION_NEXT_CHAPTER = 9,
  ACTION_PREVIOUS_SUBTITLE = 10,
  ACTION_NEXT_SUBTITLE = 11,
  ACTION_TOGGLE_SUBTITLE = 12,
  ACTION_DECREASE_SUBTITLE_DELAY = 13,
  ACTION_INCREASE_SUBTITLE_DELAY = 14,
  ACTION_EXIT = 15,
  ACTION_PLAYPAUSE = 16,
  ACTION_DECREASE_VOLUME = 17,
  ACTION_INCREASE_VOLUME = 18,
  ACTION_SEEK_BACK_SMALL = 19,
  ACTION_SEEK_FORWARD_SMALL = 20,
  ACTION_SEEK_BACK_LARGE = 21,
  ACTION_SEEK_FORWARD_LARGE = 22,
  ACTION_STEP = 23,
  ACTION_BLANK = 24,
  ACTION_SEEK_RELATIVE = 25,
  //ACTION_SEEK_ABSOLUTE = 26, (needs param)
  //ACTION_MOVE_VIDEO = 27, (needs param)
  ACTION_HIDE_VIDEO = 28,
  ACTION_UNHIDE_VIDEO = 29,
  ACTION_HIDE_SUBTITLES = 30,
  ACTION_SHOW_SUBTITLES = 31,
  //ACTION_SET_ALPHA = 32, (needs param)
  //ACTION_SET_ASPECT_MODE = 33, (needs param)
  //ACTION_CROP_VIDEO = 34, (needs param)
  ACTION_PAUSE = 35,
  ACTION_PLAY = 36,
  //ACTION_CHANGE_FILE = 37,
  //ACTION_SET_LAYER = 38, (needs param)

  ACTION_PREVIOUS_FILE = 39,
  ACTION_NEXT_FILE = 40,

  ACTION_MUTE = 41,
  ACTION_UNMUTE = 42,

  // From here on the methods either require dbus parameters or provide dbus
  // output so they can't work with keyboard input or the Action method
  START_OF_DBUS_METHODS = 100,

  INVALID_METHOD,
  INVALID_PROPERTY,

  CAN_CONTROL,
  CAN_GO_FULLSCREEN,
  CAN_GO_NEXT,
  CAN_GO_PREVIOUS,
  CAN_PAUSE,
  CAN_PLAY,
  CAN_QUIT,
  CAN_RAISE,
  CAN_SEEK,
  CAN_SET_FULLSCREEN,
  DO_ACTION,
  GET,
  GET_ASPECT,
  GET_CAN_RAISE,
  GET_DURATION,
  GET_FULLSCREEN,
  GET_HAS_TRACK_LIST,
  GET_IDENTITY,
  GET_MAXIMUM_RATE,
  GET_METADATA,
  GET_MINIMUM_RATE,
  GET_PLAYBACK_STATUS,
  GET_POSITION,
  GET_RATE,
  GET_RES_HEIGHT,
  GET_RES_WIDTH,
  GET_SOURCE,
  GET_SUPPORTED_MIME_TYPES,
  GET_SUPPORTED_URI_SCHEMES,
  GET_VIDEO_STREAM_COUNT,
  GET_VOLUME,
  HAS_TRACK_LIST,
  LIST_AUDIO,
  LIST_SUBTITLES,
  LIST_VIDEO,
  OPEN_URI,
  RAISE,
  SET,
  SET_ALPHA,
  SET_ASPECT_MODE,
  SET_AUDIO_STREAM,
  SET_LAYER,
  SET_POSITION,
  SET_SPEED,
  SET_SUBTITLE_STREAM,
  SET_VOLUME,
};

#define KEY_LEFT 0x5b44
#define KEY_RIGHT 0x5b43
#define KEY_UP 0x5b41
#define KEY_DOWN 0x5b42
#define KEY_ESC 27

namespace KeyConfig
{
  void buildKeymap(const char *filepath, std::unordered_map<int, int> &keymap);
}
