#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dbus/dbus.h>

#include "OMXControl.h"
#include "DbusCommandSearch.h"

struct lookup_item {
	const char *k;
	enum DBusMethod v;
};

const struct lookup_item get_play_table[] = {
  {"Aspect",                GET_PLAYER_ASPECT},
  {"CanControl",            GET_PLAYER_CAN_CONTROL},
  {"CanGoNext",             GET_PLAYER_CAN_GO_NEXT},
  {"CanGoPrevious",         GET_PLAYER_CAN_GO_PREVIOUS},
  {"CanPause",              GET_PLAYER_CAN_PAUSE},
  {"CanPlay",               GET_PLAYER_CAN_PLAY},
  {"CanSeek",               GET_PLAYER_CAN_SEEK},
  {"Duration",              GET_PLAYER_DURATION},
  {"MaximumRate",           GET_PLAYER_MAXIMUM_RATE},
  {"Metadata",              GET_PLAYER_METADATA},
  {"MinimumRate",           GET_PLAYER_MINIMUM_RATE},
  {"PlaybackStatus",        GET_PLAYER_PLAYBACK_STATUS},
  {"Position",              GET_PLAYER_POSITION},
  {"Rate",                  GET_PLAYER_RATE},
  {"ResHeight",             GET_PLAYER_RES_HEIGHT},
  {"ResWidth",              GET_PLAYER_RES_WIDTH},
  {"VideoStreamCount",      GET_PLAYER_VIDEO_STREAM_COUNT},
  {"Volume",                GET_PLAYER_VOLUME},
};

const struct lookup_item get_root_table[] = {
  {"CanQuit",               GET_ROOT_CAN_QUIT},
  {"CanRaise",              GET_ROOT_CAN_RAISE},
  {"CanSetFullscreen",      GET_ROOT_CAN_SET_FULLSCREEN},
  {"Fullscreen",            GET_ROOT_FULLSCREEN},
  {"HasTrackList",          GET_ROOT_HAS_TRACK_LIST},
  {"Identity",              GET_ROOT_IDENTITY},
  {"SupportedMimeTypes",    GET_ROOT_SUPPORTED_MIME_TYPES},
  {"SupportedUriSchemes",   GET_ROOT_SUPPORTED_URI_SCHEMES},
};

const struct lookup_item play_table[] = {
  {"Action",                    PLAYER_ACTION},
  {"GetSource",                 PLAYER_GET_SOURCE},
  {"HideSubtitles",             PLAYER_HIDE_SUBTITLES},
  {"HideVideo",                 PLAYER_HIDE_VIDEO},
  {"ListAudio",                 PLAYER_LIST_AUDIO},
  {"ListSubtitles",             PLAYER_LIST_SUBTITLES},
  {"ListVideo",                 PLAYER_LIST_VIDEO},
  {"Mute",                      PLAYER_MUTE},
  {"Next",                      PLAYER_NEXT},
  {"OpenUri",                   PLAYER_OPEN_URI},
  {"Pause",                     PLAYER_PAUSE},
  {"Play",                      PLAYER_PLAY},
  {"PlayPause",                 PLAYER_PLAY_PAUSE},
  {"Previous",                  PLAYER_PREVIOUS},
  {"Seek",                      PLAYER_SEEK},
  {"SelectAudio",               PLAYER_SELECT_AUDIO},
  {"SelectSubtitle",            PLAYER_SELECT_SUBTITLE},
  {"SetAlpha",                  PLAYER_SET_ALPHA},
  {"SetAspectMode",             PLAYER_SET_ASPECT_MODE},
  {"SetLayer",                  PLAYER_SET_LAYER},
  {"SetPosition",               PLAYER_SET_POSITION},
  {"ShowSubtitles",             PLAYER_SHOW_SUBTITLES},
  {"Stop",                      PLAYER_STOP},
  {"UnHideVideo",               PLAYER_UN_HIDE_VIDEO},
  {"Unmute",                    PLAYER_UNMUTE},
};

const struct lookup_item prop_table[] = {
  {"Aspect",                    PROP_ASPECT},
  {"CanControl",                PROP_CAN_CONTROL},
  {"CanGoNext",                 PROP_CAN_GO_NEXT},
  {"CanGoPrevious",             PROP_CAN_GO_PREVIOUS},
  {"CanPause",                  PROP_CAN_PAUSE},
  {"CanPlay",                   PROP_CAN_PLAY},
  {"CanQuit",                   PROP_CAN_QUIT},
  {"CanRaise",                  PROP_CAN_RAISE},
  {"CanSeek",                   PROP_CAN_SEEK},
  {"CanSetFullscreen",          PROP_CAN_SET_FULLSCREEN},
  {"Duration",                  PROP_DURATION},
  {"Fullscreen",                PROP_FULLSCREEN},
  {"Get",                       PROP_GET},
  {"GetSource",                 PROP_GET_SOURCE},
  {"HasTrackList",              PROP_HAS_TRACK_LIST},
  {"Identity",                  PROP_IDENTITY},
  {"MaximumRate",               PROP_MAXIMUM_RATE},
  {"MinimumRate",               PROP_MINIMUM_RATE},
  {"Mute",                      PROP_MUTE},
  {"PlaybackStatus",            PROP_PLAYBACK_STATUS},
  {"Position",                  PROP_POSITION},
  {"ResHeight",                 PROP_RES_HEIGHT},
  {"ResWidth",                  PROP_RES_WIDTH},
  {"Set",                       PROP_SET},
  {"SupportedMimeTypes",        PROP_SUPPORTED_MIME_TYPES},
  {"SupportedUriSchemes",       PROP_SUPPORTED_URI_SCHEMES},
  {"Unmute",                    PROP_UNMUTE},
  {"VideoStreamCount",          PROP_VIDEO_STREAM_COUNT},
  {"Volume",                    PROP_VOLUME},
};

const struct lookup_item root_table[] = {
  {"Quit",                      ROOT_QUIT},
  {"Raise",                     ROOT_RAISE},
};

struct interface_item {
	const char *k;
	const struct lookup_item *method_table;
	const int method_count;
	const struct lookup_item *prop_table;
	const int prop_count;
};

const struct interface_item interface_table[] = {
  {
    DBUS_INTERFACE_PROPERTIES,
    prop_table,
    sizeof(prop_table) / sizeof(lookup_item),
    NULL,
    0,
  },
  {
    OMXPLAYER_DBUS_INTERFACE_ROOT,
    root_table,
    sizeof(root_table) / sizeof(lookup_item),
    get_root_table,
    sizeof(get_root_table) / sizeof(lookup_item),
  },
  {
    OMXPLAYER_DBUS_INTERFACE_PLAYER,
    play_table,
    sizeof(play_table) / sizeof(lookup_item),
    get_play_table,
    sizeof(get_play_table) / sizeof(lookup_item),
  },
};

const int interface_count = sizeof(interface_table) / sizeof(interface_item);

static int cmp_interface(const void *a, const void *b)
{
  const struct interface_item *aa = (const struct interface_item *)a;
  const struct interface_item *bb = (const struct interface_item *)b;

  return strcmp(aa->k, bb->k);
}


static const struct interface_item *find_interface(const char *search_string)
{
	struct interface_item needle = {search_string};
	return (struct interface_item *)bsearch(&needle,
	                                        interface_table,
	                                        interface_count,
	                                        sizeof(struct interface_item),
	                                        cmp_interface);
}


static int cmp_item(const void *a, const void *b)
{
  const struct lookup_item *aa = (const struct lookup_item *)a;
  const struct lookup_item *bb = (const struct lookup_item *)b;

  return strcmp(aa->k, bb->k);
}

enum DBusMethod dbus_find_method(const char *interface_name, const char *method_name)
{
	const struct interface_item *interface = find_interface(interface_name);
	if(interface == NULL)
		return INVALID_INTERFACE;

	struct lookup_item needle = {.k = method_name};
	struct lookup_item *method = static_cast<struct lookup_item*>(
		bsearch(&needle,
				interface->method_table,
				interface->method_count,
				sizeof(struct lookup_item),
				cmp_item));

	if(method == NULL)
		return INVALID_METHOD;

	return method->v;
}

enum DBusMethod dbus_find_property(const char *interface_name, const char *prop_name)
{
	const struct interface_item *interface = find_interface(interface_name);
	if(interface == NULL)
		return INVALID_INTERFACE;

	if(interface->prop_table == NULL)
		return INVALID_METHOD;

	struct lookup_item needle = {.k = prop_name};
	struct lookup_item *method = static_cast<struct lookup_item*>(
		bsearch(&needle,
				interface->prop_table,
				interface->prop_count,
				sizeof(struct lookup_item),
				cmp_item));

	if(method == NULL)
		return INVALID_METHOD;

	return method->v;
}
