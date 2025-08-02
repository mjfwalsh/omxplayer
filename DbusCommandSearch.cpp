/*
 * Copyright (C) 2022 by Michael J. Walsh
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

#include <string.h>
#include <stdlib.h>

#include "DbusCommandSearch.h"

struct lookup_item {
  const char *key;
  enum Action method;
  enum Action property;
};

const struct lookup_item table[] = {
// NB this list is CASE SENSITIVELY sorted
{"Action",              DO_ACTION,                 INVALID_PROPERTY},
{"Aspect",              GET_ASPECT,                GET_ASPECT},
{"CanControl",          CAN_CONTROL,               CAN_CONTROL},
{"CanGoNext",           CAN_GO_NEXT,               CAN_GO_NEXT},
{"CanGoPrevious",       CAN_GO_PREVIOUS,           CAN_GO_PREVIOUS},
{"CanPause",            CAN_PAUSE,                 CAN_PAUSE},
{"CanPlay",             CAN_PLAY,                  CAN_PLAY},
{"CanQuit",             CAN_QUIT,                  CAN_QUIT},
{"CanRaise",            CAN_RAISE,                 GET_CAN_RAISE},
{"CanSeek",             CAN_SEEK,                  CAN_SEEK},
{"CanSetFullscreen",    CAN_SET_FULLSCREEN,        CAN_SET_FULLSCREEN},
{"Duration",            GET_DURATION,              GET_DURATION},
{"Fullscreen",          CAN_GO_FULLSCREEN,         GET_FULLSCREEN},
{"Get",                 GET,                       INVALID_PROPERTY},
{"GetSource",           GET_SOURCE,                INVALID_PROPERTY},
{"HasTrackList",        HAS_TRACK_LIST,            GET_HAS_TRACK_LIST},
{"HideSubtitles",       ACTION_HIDE_SUBTITLES,     INVALID_PROPERTY},
{"HideVideo",           ACTION_HIDE_VIDEO,         INVALID_PROPERTY},
{"Identity",            GET_IDENTITY,              GET_IDENTITY},
{"ListAudio",           LIST_AUDIO,                INVALID_PROPERTY},
{"ListSubtitles",       LIST_SUBTITLES,            INVALID_PROPERTY},
{"ListVideo",           LIST_VIDEO,                INVALID_PROPERTY},
{"MaximumRate",         GET_MAXIMUM_RATE,          GET_MAXIMUM_RATE},
{"Metadata",            INVALID_METHOD,            GET_METADATA},
{"MinimumRate",         GET_MINIMUM_RATE,          GET_MINIMUM_RATE},
{"Mute",                ACTION_MUTE,               INVALID_PROPERTY},
{"Next",                ACTION_NEXT_CHAPTER,       INVALID_PROPERTY},
{"OpenUri",             OPEN_URI,                  INVALID_PROPERTY},
{"Pause",               ACTION_PAUSE,              INVALID_PROPERTY},
{"Play",                ACTION_PLAY,               INVALID_PROPERTY},
{"PlayPause",           ACTION_PLAYPAUSE,          INVALID_PROPERTY},
{"PlaybackStatus",      GET_PLAYBACK_STATUS,       GET_PLAYBACK_STATUS},
{"Position",            GET_POSITION,              GET_POSITION},
{"Previous",            ACTION_PREVIOUS_CHAPTER,   INVALID_PROPERTY},
{"Quit",                ACTION_EXIT,               INVALID_PROPERTY},
{"Raise",               RAISE,                     INVALID_PROPERTY},
{"Rate",                INVALID_METHOD,            GET_RATE},
{"ResHeight",           GET_RES_HEIGHT,            GET_RES_HEIGHT},
{"ResWidth",            GET_RES_WIDTH,             GET_RES_WIDTH},
{"Seek",                ACTION_SEEK_RELATIVE,      INVALID_PROPERTY},
{"SelectAudio",         SET_AUDIO_STREAM,          INVALID_PROPERTY},
{"SelectSubtitle",      SET_SUBTITLE_STREAM,       INVALID_PROPERTY},
{"Set",                 SET,                       INVALID_PROPERTY},
{"SetAlpha",            SET_ALPHA,                 INVALID_PROPERTY},
{"SetAspectMode",       SET_ASPECT_MODE,           INVALID_PROPERTY},
{"SetLayer",            SET_LAYER,                 INVALID_PROPERTY},
{"SetPosition",         SET_POSITION,              INVALID_PROPERTY},
{"ShowSubtitles",       ACTION_SHOW_SUBTITLES,     INVALID_PROPERTY},
{"Stop",                ACTION_EXIT,               INVALID_PROPERTY},
{"SupportedMimeTypes",  GET_SUPPORTED_MIME_TYPES,  GET_SUPPORTED_MIME_TYPES},
{"SupportedUriSchemes", GET_SUPPORTED_URI_SCHEMES, GET_SUPPORTED_URI_SCHEMES},
{"UnHideVideo",         ACTION_UNHIDE_VIDEO,       INVALID_PROPERTY},
{"Unmute",              ACTION_UNMUTE,             INVALID_PROPERTY},
{"VideoStreamCount",    GET_VIDEO_STREAM_COUNT,    GET_VIDEO_STREAM_COUNT},
{"Volume",              SET_VOLUME,                GET_VOLUME},
};

static int cmp_item(const void *a, const void *b)
{
  const char *aa = (const char *)a;
  const struct lookup_item *bb = (const struct lookup_item *)b;

  return strcmp(aa, bb->key);
}

static struct lookup_item *search_table(const char *needle)
{
  return static_cast<struct lookup_item*>(
    bsearch(needle,
        table,
        sizeof(table) / sizeof(lookup_item),
        sizeof(struct lookup_item),
        cmp_item)
  );
}


enum Action dbus_find_method(const char *method_name)
{
  const struct lookup_item *method = search_table(method_name);
  if(method == nullptr)
    return INVALID_METHOD;

  return method->method;
}

enum Action dbus_find_property(const char *property_name)
{
  const struct lookup_item *property = search_table(property_name);
  if(property == nullptr)
    return INVALID_PROPERTY;

  return property->property;
}
