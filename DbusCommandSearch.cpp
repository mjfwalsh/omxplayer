#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dbus/dbus.h>

#include "OMXControl.h"
#include "DbusCommandSearch.h"

struct lookup_item {
	const char *key;
	enum DBusMethod method;
	enum DBusMethod property;
};

const struct lookup_item table[] = {
// NB this list is CASE SENSITIVELY sorted
{"Action",              ACTION,                INVALID_PROPERTY},
{"Aspect",              ASPECT,                ASPECT},
{"CanControl",          CAN_CONTROL,           CAN_CONTROL},
{"CanGoNext",           CAN_GO_NEXT,           CAN_GO_NEXT},
{"CanGoPrevious",       CAN_GO_PREVIOUS,       CAN_GO_PREVIOUS},
{"CanPause",            CAN_PAUSE,             CAN_PAUSE},
{"CanPlay",             CAN_PLAY,              CAN_PLAY},
{"CanQuit",             CAN_QUIT,              CAN_QUIT},
{"CanRaise",            CAN_RAISE,             GET_ROOT_CAN_RAISE},
{"CanSeek",             CAN_SEEK,              CAN_SEEK},
{"CanSetFullscreen",    CAN_SET_FULLSCREEN,    CAN_SET_FULLSCREEN},
{"Duration",            DURATION,              DURATION},
{"Fullscreen",          FULLSCREEN,            GET_ROOT_FULLSCREEN},
{"Get",                 GET,                   INVALID_PROPERTY},
{"GetSource",           GET_SOURCE,            INVALID_PROPERTY},
{"HasTrackList",        HAS_TRACK_LIST,        GET_ROOT_HAS_TRACK_LIST},
{"HideSubtitles",       HIDE_SUBTITLES,        INVALID_PROPERTY},
{"HideVideo",           HIDE_VIDEO,            INVALID_PROPERTY},
{"Identity",            IDENTITY,              IDENTITY},
{"ListAudio",           LIST_AUDIO,            INVALID_PROPERTY},
{"ListSubtitles",       LIST_SUBTITLES,        INVALID_PROPERTY},
{"ListVideo",           LIST_VIDEO,            INVALID_PROPERTY},
{"MaximumRate",         MAXIMUM_RATE,          GET_PLAYER_MAXIMUM_RATE},
{"Metadata",            INVALID_METHOD,        GET_PLAYER_METADATA},
{"MinimumRate",         MINIMUM_RATE,          GET_PLAYER_MINIMUM_RATE},
{"Mute",                MUTE,                  INVALID_PROPERTY},
{"Next",                NEXT,                  INVALID_PROPERTY},
{"OpenUri",             OPEN_URI,              INVALID_PROPERTY},
{"Pause",               PAUSE,                 INVALID_PROPERTY},
{"Play",                PLAY,                  INVALID_PROPERTY},
{"PlayPause",           PLAY_PAUSE,            INVALID_PROPERTY},
{"PlaybackStatus",      PLAYBACK_STATUS,       PLAYBACK_STATUS},
{"Position",            POSITION,              POSITION},
{"Previous",            PREVIOUS,              INVALID_PROPERTY},
{"Quit",                QUIT,                  INVALID_PROPERTY},
{"Raise",               RAISE,                 INVALID_PROPERTY},
{"Rate",                INVALID_METHOD,        GET_PLAYER_RATE},
{"ResHeight",           RES_HEIGHT,            RES_HEIGHT},
{"ResWidth",            RES_WIDTH,             RES_WIDTH},
{"Seek",                SEEK,                  INVALID_PROPERTY},
{"SelectAudio",         SELECT_AUDIO,          INVALID_PROPERTY},
{"SelectSubtitle",      SELECT_SUBTITLE,       INVALID_PROPERTY},
{"Set",                 SET,                   INVALID_PROPERTY},
{"SetAlpha",            SET_ALPHA,             INVALID_PROPERTY},
{"SetAspectMode",       SET_ASPECT_MODE,       INVALID_PROPERTY},
{"SetLayer",            SET_LAYER,             INVALID_PROPERTY},
{"SetPosition",         SET_POSITION,          INVALID_PROPERTY},
{"ShowSubtitles",       SHOW_SUBTITLES,        INVALID_PROPERTY},
{"Stop",                STOP,                  INVALID_PROPERTY},
{"SupportedMimeTypes",  SUPPORTED_MIME_TYPES,  SUPPORTED_MIME_TYPES},
{"SupportedUriSchemes", SUPPORTED_URI_SCHEMES, SUPPORTED_URI_SCHEMES},
{"UnHideVideo",         UN_HIDE_VIDEO,         INVALID_PROPERTY},
{"Unmute",              UNMUTE,                INVALID_PROPERTY},
{"VideoStreamCount",    VIDEO_STREAM_COUNT,    VIDEO_STREAM_COUNT},
{"Volume",              VOLUME,                GET_PLAYER_VOLUME},
};

static int cmp_item(const void *a, const void *b)
{
  const struct lookup_item *aa = (const struct lookup_item *)a;
  const struct lookup_item *bb = (const struct lookup_item *)b;

  return strcmp(aa->key, bb->key);
}

struct lookup_item *search_table(const char *name)
{
	struct lookup_item needle = {.key = name};
	return static_cast<struct lookup_item*>(
		bsearch(&needle,
				table,
				sizeof(table) / sizeof(lookup_item),
				sizeof(struct lookup_item),
				cmp_item)
	);
}


enum DBusMethod dbus_find_method(const char *method_name)
{
	struct lookup_item *method = search_table(method_name);

	if(method == NULL)
		return INVALID_METHOD;

	return method->method;
}

enum DBusMethod dbus_find_property(const char *property_name)
{
	struct lookup_item *property = search_table(property_name);

	if(property == NULL)
		return INVALID_PROPERTY;

	return property->property;
}
