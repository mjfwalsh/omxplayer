#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>

#include <string>
#include <unordered_map>

#include "utils/log.h"
#include "OMXClock.h"
#include "OMXControl.h"
#include "KeyConfig.h"
#include "OMXPlayerAudio.h"
#include "OMXReader.h"
#include "OMXPlayerSubtitles.h"
#include "utils/misc.h"


template <class T>
static bool dbus_message_get_arg(DBusMessage *m, int type, T *value)
{
  DBusError error;
  dbus_error_init(&error);

  dbus_message_get_args(m, &error, type, value, DBUS_TYPE_INVALID);

  if(dbus_error_is_set(&error))
  {
    dbus_error_free(&error);
    return false;
  }
  else
  {
    return true;
  }
}

static void ToURI(const std::string& str, char *uri)
{
  //Build URI if needed
  if(IsURL(str))
  {
    //Just write URL as it is
    strncpy(uri, str.c_str(), PATH_MAX);
  }
  else
  {
    //Get file full path and add file://
    char * real_path=realpath(str.c_str(), NULL);
    sprintf(uri, "file://%s", real_path);
    free(real_path);
  }
}

#define CLASSNAME "OMXControl"

OMXControlResult::OMXControlResult( int newKey ) {
  key = newKey;
}

OMXControlResult::OMXControlResult( int newKey, int64_t newArg ) {
  key = newKey;
  arg = newArg;
}

OMXControlResult::OMXControlResult( int newKey, const char *newArg ) {
  key = newKey;
  winarg = newArg;
}

int OMXControlResult::getKey() {
  return key;
}

int64_t OMXControlResult::getArg() {
  return arg;
}

const char *OMXControlResult::getWinArg() {
  return winarg;
}

OMXControl::OMXControl()
{
  interfaces[DBUS_INTERFACE_PROPERTIES] = "Prop";
  interfaces[OMXPLAYER_DBUS_INTERFACE_ROOT] = "Root";
  interfaces[OMXPLAYER_DBUS_INTERFACE_PLAYER] = "Play";

  table["Root|Quit"] = ROOT_QUIT;
  table["Root|Raise"] = ROOT_RAISE;
  table["Prop|Get"] = PROP_GET;
  table["Prop|Set"] = PROP_SET;
  table["Prop|CanQuit"] = PROP_CAN_QUIT;
  table["Prop|Fullscreen"] = PROP_FULLSCREEN;
  table["Prop|CanSetFullscreen"] = PROP_CAN_SET_FULLSCREEN;
  table["Prop|CanRaise"] = PROP_CAN_RAISE;
  table["Prop|HasTrackList"] = PROP_HAS_TRACK_LIST;
  table["Prop|Identity"] = PROP_IDENTITY;
  table["Prop|SupportedUriSchemes"] = PROP_SUPPORTED_URI_SCHEMES;
  table["Prop|SupportedMimeTypes"] = PROP_SUPPORTED_MIME_TYPES;
  table["Prop|CanGoNext"] = PROP_CAN_GO_NEXT;
  table["Prop|CanGoPrevious"] = PROP_CAN_GO_PREVIOUS;
  table["Prop|CanSeek"] = PROP_CAN_SEEK;
  table["Prop|CanControl"] = PROP_CAN_CONTROL;
  table["Prop|CanPlay"] = PROP_CAN_PLAY;
  table["Prop|CanPause"] = PROP_CAN_PAUSE;
  table["Prop|PlaybackStatus"] = PROP_PLAYBACK_STATUS;
  table["Prop|GetSource"] = PROP_GET_SOURCE;
  table["Prop|Volume"] = PROP_VOLUME;
  table["Prop|Mute"] = PROP_MUTE;
  table["Prop|Unmute"] = PROP_UNMUTE;
  table["Prop|Position"] = PROP_POSITION;
  table["Prop|Aspect"] = PROP_ASPECT;
  table["Prop|VideoStreamCount"] = PROP_VIDEO_STREAM_COUNT;
  table["Prop|ResWidth"] = PROP_RES_WIDTH;
  table["Prop|ResHeight"] = PROP_RES_HEIGHT;
  table["Prop|Duration"] = PROP_DURATION;
  table["Prop|MinimumRate"] = PROP_MINIMUM_RATE;
  table["Prop|MaximumRate"] = PROP_MAXIMUM_RATE;
  table["Play|GetSource"] = PLAYER_GET_SOURCE;
  table["Play|Next"] = PLAYER_NEXT;
  table["Play|Previous"] = PLAYER_PREVIOUS;
  table["Play|Pause"] = PLAYER_PAUSE;
  table["Play|Play"] = PLAYER_PLAY;
  table["Play|PlayPause"] = PLAYER_PLAY_PAUSE;
  table["Play|Stop"] = PLAYER_STOP;
  table["Play|Seek"] = PLAYER_SEEK;
  table["Play|SetPosition"] = PLAYER_SET_POSITION;
  table["Play|SetAlpha"] = PLAYER_SET_ALPHA;
  table["Play|SetLayer"] = PLAYER_SET_LAYER;
  table["Play|SetAspectMode"] = PLAYER_SET_ASPECT_MODE;
  table["Play|Mute"] = PLAYER_MUTE;
  table["Play|Unmute"] = PLAYER_UNMUTE;
  table["Play|ListSubtitles"] = PLAYER_LIST_SUBTITLES;
  table["Play|HideVideo"] = PLAYER_HIDE_VIDEO;
  table["Play|UnHideVideo"] = PLAYER_UN_HIDE_VIDEO;
  table["Play|ListAudio"] = PLAYER_LIST_AUDIO;
  table["Play|ListVideo"] = PLAYER_LIST_VIDEO;
  table["Play|SelectSubtitle"] = PLAYER_SELECT_SUBTITLE;
  table["Play|SelectAudio"] = PLAYER_SELECT_AUDIO;
  table["Play|ShowSubtitles"] = PLAYER_SHOW_SUBTITLES;
  table["Play|HideSubtitles"] = PLAYER_HIDE_SUBTITLES;
  table["Play|OpenUri"] = PLAYER_OPEN_URI;
  table["Play|Action"] = PLAYER_ACTION;

  // get properties
  table["Get|Root|CanRaise"] = GET_ROOT_CAN_RAISE;
  table["Get|Root|CanQuit"] = GET_ROOT_CAN_QUIT;
  table["Get|Root|CanSetFullscreen"] = GET_ROOT_CAN_SET_FULLSCREEN;
  table["Get|Root|Fullscreen"] = GET_ROOT_FULLSCREEN;
  table["Get|Root|HasTrackList"] = GET_ROOT_HAS_TRACK_LIST;
  table["Get|Root|Identity"] = GET_ROOT_IDENTITY;
  table["Get|Root|SupportedUriSchemes"] = GET_ROOT_SUPPORTED_URI_SCHEMES;
  table["Get|Root|SupportedMimeTypes"] = GET_ROOT_SUPPORTED_MIME_TYPES;
  table["Get|Play|CanGoNext"] = GET_PLAYER_CAN_GO_NEXT;
  table["Get|Play|CanGoPrevious"] = GET_PLAYER_CAN_GO_PREVIOUS;
  table["Get|Play|CanSeek"] = GET_PLAYER_CAN_SEEK;
  table["Get|Play|CanControl"] = GET_PLAYER_CAN_CONTROL;
  table["Get|Play|CanPlay"] = GET_PLAYER_CAN_PLAY;
  table["Get|Play|CanPause"] = GET_PLAYER_CAN_PAUSE;
  table["Get|Play|Position"] = GET_PLAYER_POSITION;
  table["Get|Play|PlaybackStatus"] = GET_PLAYER_PLAYBACK_STATUS;
  table["Get|Play|MinimumRate"] = GET_PLAYER_MINIMUM_RATE;
  table["Get|Play|MaximumRate"] = GET_PLAYER_MAXIMUM_RATE;
  table["Get|Play|Rate"] = GET_PLAYER_RATE;
  table["Get|Play|Volume"] = GET_PLAYER_VOLUME;
  table["Get|Play|Metadata"] = GET_PLAYER_METADATA;
  table["Get|Play|Aspect"] = GET_PLAYER_ASPECT;
  table["Get|Play|VideoStreamCount"] = GET_PLAYER_VIDEO_STREAM_COUNT;
  table["Get|Play|ResWidth"] = GET_PLAYER_RES_WIDTH;
  table["Get|Play|ResHeight"] = GET_PLAYER_RES_HEIGHT;
  table["Get|Play|Duration"] = GET_PLAYER_DURATION;

}

OMXControl::~OMXControl() 
{
  dbus_disconnect();
}

void OMXControl::init(OMXClock *av_clock, OMXPlayerSubtitles *player_subtitles, std::string (*filename)())
{
  clock        = av_clock;
  subtitles    = player_subtitles;
  get_filename = filename;
}


void OMXControl::set_reader(OMXReader *omx_reader)
{
  reader       = omx_reader;
}

void OMXControl::set_audio(OMXPlayerAudio *player_audio)
{
  audio        = player_audio;
}

bool OMXControl::connect(std::string& dbus_name)
{
  if (dbus_connect(dbus_name) < 0)
  {
    CLogLog(LOGWARNING, "DBus connection failed, trying alternate");
    dbus_disconnect();
    dbus_name += ".instance";
    dbus_name += std::to_string(getpid());
    if (dbus_connect(dbus_name) < 0)
    {
      CLogLog(LOGWARNING, "DBus connection failed, alternate failed, will continue without DBus");
      dbus_disconnect();
      return false;
    } else {
      CLogLog(LOGDEBUG, "DBus connection succeeded");
      dbus_threads_init_default();
    }
  }
  else
  {
    CLogLog(LOGDEBUG, "DBus connection succeeded");
    dbus_threads_init_default();
  }
  return true;
}

void OMXControl::dispatch()
{
  if (bus)
    dbus_connection_read_write(bus, 0);
}

int OMXControl::dbus_connect(std::string& dbus_name)
{
  DBusError error;

  dbus_error_init(&error);
  if (!(bus = dbus_bus_get_private(DBUS_BUS_SESSION, &error)))
  {
    CLogLog(LOGWARNING, "dbus_bus_get_private(): %s", error.message);
        goto fail;
  }

  dbus_connection_set_exit_on_disconnect(bus, FALSE);

  if (dbus_bus_request_name(
        bus,
        dbus_name.c_str(),
        DBUS_NAME_FLAG_DO_NOT_QUEUE,
        &error) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
  {
        if (dbus_error_is_set(&error))
        {
            CLogLog(LOGWARNING, "dbus_bus_request_name(): %s", error.message);
            goto fail;
        }

        CLogLog(LOGWARNING, "Failed to acquire D-Bus name '%s'", dbus_name.c_str());
        goto fail;
    }

    return 0;

fail:
    if (dbus_error_is_set(&error))
        dbus_error_free(&error);

    if (bus)
    {
        dbus_connection_close(bus);
        dbus_connection_unref(bus);
        bus = NULL;
    }

    return -1;
}

void OMXControl::dbus_disconnect()
{
    if (bus)
    {
        dbus_connection_close(bus);
        dbus_connection_unref(bus);
        bus = NULL;
    }
}

OMXControlResult OMXControl::getEvent()
{
  if (!bus)
    return KeyConfig::ACTION_BLANK;

  dispatch();
  DBusMessage *m = dbus_connection_pop_message(bus);

  if (m == NULL)
    return KeyConfig::ACTION_BLANK;

  CLogLog(LOGDEBUG, "Popped message member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );

  std::string key;
  try {
    const char *interface = dbus_message_get_interface(m);
    const char *method = dbus_message_get_member(m);

    key = interfaces.at(interface) + "|";
    key += method;
  }
  catch(std::out_of_range const&)
  {
    CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
    dbus_respond_error(m, DBUS_ERROR_UNKNOWN_INTERFACE, "Unknown interface");
    return KeyConfig::ACTION_BLANK;
  }

  try {
    OMXControlResult result = handle_event(m, table.at(key));
    dbus_message_unref(m);

    return result;
  }
  catch(std::out_of_range const&)
  {
    CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
    dbus_respond_error(m, DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
    return KeyConfig::ACTION_BLANK;
  }
}

OMXControlResult OMXControl::handle_event(DBusMessage *m, enum DBusFunction search_key)
{
  switch(search_key)
  {
  //----------------------------DBus root interface-----------------------------
  //Methods:
  case ROOT_QUIT: // OMXPLAYER_DBUS_INTERFACE_ROOT - Quit
    dbus_respond_ok(m);//Note: No reply according to MPRIS2 specs
    return KeyConfig::ACTION_EXIT;

  case ROOT_RAISE: // OMXPLAYER_DBUS_INTERFACE_ROOT - Raise
    //Does nothing
    return KeyConfig::ACTION_BLANK;
  case PROP_GET: // DBUS_INTERFACE_PROPERTIES - Get
    {
      DBusError error;
      dbus_error_init(&error);
      std::string key;

      //Retrieve interface and property name
      const char *prop_interface, *property;
      dbus_message_get_args(m, &error, DBUS_TYPE_STRING, &prop_interface, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID);

      try {
        key = "Get|" + interfaces.at(prop_interface) + "|";
      }
      catch(std::out_of_range const&)
      {
        CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
        dbus_respond_error(m, DBUS_ERROR_UNKNOWN_INTERFACE, "Unknown interface");
        return KeyConfig::ACTION_BLANK;
      }

      try {
        key += property;
        return handle_event(m, table.at(key));
      }
      catch(std::out_of_range const&)
      {
        CLogLog(LOGWARNING, "Unhandled dbus property message, member: %s interface: %s type: %d path: %s  property: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m), property );
        dbus_respond_error(m, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
        return KeyConfig::ACTION_BLANK;
      }
    }

  //Properties Set method
  case PROP_SET: // DBUS_INTERFACE_PROPERTIES - Set
    return SetProperty(m);

  // Property access
  case PROP_CAN_QUIT: // DBUS_INTERFACE_PROPERTIES - CanQuit
  case GET_ROOT_CAN_QUIT: // OMXPLAYER_DBUS_INTERFACE_ROOT - CanQuit
  case PROP_FULLSCREEN: // DBUS_INTERFACE_PROPERTIES - Fullscreen
    dbus_respond_boolean(m, 1);
    return KeyConfig::ACTION_BLANK;

  case PROP_CAN_SET_FULLSCREEN: // DBUS_INTERFACE_PROPERTIES - CanSetFullscreen
  case PROP_CAN_RAISE: // DBUS_INTERFACE_PROPERTIES - CanRaise
  case PROP_HAS_TRACK_LIST: // DBUS_INTERFACE_PROPERTIES - HasTrackList
    dbus_respond_boolean(m, 0);
    return KeyConfig::ACTION_BLANK;

  case PROP_IDENTITY: // DBUS_INTERFACE_PROPERTIES - Identity
  case GET_ROOT_IDENTITY: // OMXPLAYER_DBUS_INTERFACE_ROOT - Identity
    dbus_respond_string(m, "OMXPlayer");
    return KeyConfig::ACTION_BLANK;

  case PROP_SUPPORTED_URI_SCHEMES: // DBUS_INTERFACE_PROPERTIES - SupportedUriSchemes
  case GET_ROOT_SUPPORTED_URI_SCHEMES: // OMXPLAYER_DBUS_INTERFACE_ROOT - SupportedUriSchemes //TODO: Update ?
    {
      const char *UriSchemes[] = {"file", "http", "rtsp", "rtmp"};
      dbus_respond_array(m, UriSchemes, 4); // Array is of length 4
      return KeyConfig::ACTION_BLANK;
    }
  case PROP_SUPPORTED_MIME_TYPES: // DBUS_INTERFACE_PROPERTIES - SupportedMimeTypes
  case GET_ROOT_SUPPORTED_MIME_TYPES: // OMXPLAYER_DBUS_INTERFACE_ROOT - SupportedMimeTypes //Vinc: TODO: Minimal list of supported types based on ffmpeg minimal support ?
    {
      const char *MimeTypes[] = {}; // Needs supplying
      dbus_respond_array(m, MimeTypes, 0);
      return KeyConfig::ACTION_BLANK;
    }
  case PROP_CAN_GO_NEXT: // DBUS_INTERFACE_PROPERTIES - CanGoNext
  case PROP_CAN_GO_PREVIOUS: // DBUS_INTERFACE_PROPERTIES - CanGoPrevious
  case GET_PLAYER_CAN_GO_NEXT: // OMXPLAYER_DBUS_INTERFACE_PLAYER - CanGoNext
  case GET_PLAYER_CAN_GO_PREVIOUS: // OMXPLAYER_DBUS_INTERFACE_PLAYER - CanGoPrevious
    dbus_respond_boolean(m, 0);
    return KeyConfig::ACTION_BLANK;

  case PROP_CAN_SEEK: // DBUS_INTERFACE_PROPERTIES - CanSeek
  case GET_PLAYER_CAN_SEEK: // OMXPLAYER_DBUS_INTERFACE_PLAYER - CanSeek
    dbus_respond_boolean(m, reader->CanSeek());
    return KeyConfig::ACTION_BLANK;

  case PROP_CAN_CONTROL: // DBUS_INTERFACE_PROPERTIES - CanControl
  case PROP_CAN_PLAY: // DBUS_INTERFACE_PROPERTIES - CanPlay
  case PROP_CAN_PAUSE: // DBUS_INTERFACE_PROPERTIES - CanPause
  case GET_PLAYER_CAN_CONTROL: // OMXPLAYER_DBUS_INTERFACE_PLAYER - CanControl
  case GET_PLAYER_CAN_PLAY: // OMXPLAYER_DBUS_INTERFACE_PLAYER - CanPlay
  case GET_PLAYER_CAN_PAUSE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - CanPause
    dbus_respond_boolean(m, 1);
    return KeyConfig::ACTION_BLANK;

  case PROP_PLAYBACK_STATUS: // DBUS_INTERFACE_PROPERTIES - PlaybackStatus
  case GET_PLAYER_PLAYBACK_STATUS: // OMXPLAYER_DBUS_INTERFACE_PLAYER - PlaybackStatus
    {
      const char *status;
      if (clock->OMXIsPaused())
      {
        status = "Paused";
      }
      else
      {
        status = "Playing";
      }

      dbus_respond_string(m, status);
      return KeyConfig::ACTION_BLANK;
    }
  case PROP_GET_SOURCE: // DBUS_INTERFACE_PROPERTIES - GetSource
    dbus_respond_string(m, get_filename().c_str());
      return KeyConfig::ACTION_BLANK;

  case PROP_VOLUME: // DBUS_INTERFACE_PROPERTIES - Volume
    {
      if(!audio)
      {
        dbus_respond_double(m, 0.0);
        return KeyConfig::ACTION_BLANK;
      }

      double volume;
      if (!dbus_message_get_arg(m, DBUS_TYPE_DOUBLE, &volume))
      { // i.e. Get current volume
        dbus_respond_double(m, audio->GetVolume());
        return KeyConfig::ACTION_BLANK;
      }
      else
      {
        //Min value is 0
        if(volume < 0.0)
          volume = 0.0;

        audio->SetVolume(volume);

        dbus_respond_double(m, volume);
        return KeyConfig::ACTION_BLANK;
      }
    }
  case PROP_MUTE: // DBUS_INTERFACE_PROPERTIES - Mute
    if(audio)
      audio->SetMute(true);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_BLANK;

  case PROP_UNMUTE: // DBUS_INTERFACE_PROPERTIES - Unmute
    if(audio)
      audio->SetMute(false);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_BLANK;

  case PROP_POSITION: // DBUS_INTERFACE_PROPERTIES - Position
  case GET_PLAYER_POSITION: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Position
    // Returns the current position in microseconds
    dbus_respond_int64(m, clock->OMXMediaTime());
    return KeyConfig::ACTION_BLANK;

  case PROP_ASPECT: // DBUS_INTERFACE_PROPERTIES - Aspect
  case GET_PLAYER_ASPECT: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Aspect
    // Returns aspect ratio
    dbus_respond_double(m, reader->GetAspectRatio());
    return KeyConfig::ACTION_BLANK;

  case PROP_VIDEO_STREAM_COUNT: // DBUS_INTERFACE_PROPERTIES - VideoStreamCount
  case GET_PLAYER_VIDEO_STREAM_COUNT: // OMXPLAYER_DBUS_INTERFACE_PLAYER - VideoStreamCount
    // Returns number of video streams
    dbus_respond_int64(m, reader->VideoStreamCount());
    return KeyConfig::ACTION_BLANK;

  case PROP_RES_WIDTH: // DBUS_INTERFACE_PROPERTIES - ResWidth
  case GET_PLAYER_RES_WIDTH: // OMXPLAYER_DBUS_INTERFACE_PLAYER - ResWidth
    // Returns width of video
    dbus_respond_int64(m, reader->GetWidth());
    return KeyConfig::ACTION_BLANK;

  case PROP_RES_HEIGHT: // DBUS_INTERFACE_PROPERTIES - ResHeight
  case GET_PLAYER_RES_HEIGHT: // OMXPLAYER_DBUS_INTERFACE_PLAYER - ResHeight
    // Returns height of video
    dbus_respond_int64(m, reader->GetHeight());
    return KeyConfig::ACTION_BLANK;

  case PROP_DURATION: // DBUS_INTERFACE_PROPERTIES - Duration
  case GET_PLAYER_DURATION: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Duration
    // Returns the duration in microseconds
    dbus_respond_int64(m, reader->GetStreamLengthMicro());
    return KeyConfig::ACTION_BLANK;

  case PROP_MINIMUM_RATE: // DBUS_INTERFACE_PROPERTIES - MinimumRate
    dbus_respond_double(m, 0.0);
    return KeyConfig::ACTION_BLANK;

  case PROP_MAXIMUM_RATE: // DBUS_INTERFACE_PROPERTIES - MaximumRate
    //TODO: to be made consistent
    dbus_respond_double(m, 10.125);
    return KeyConfig::ACTION_BLANK;


  //--------------------------Player interface methods--------------------------
  case PLAYER_GET_SOURCE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - GetSource
    dbus_respond_string(m, get_filename().c_str());
    return KeyConfig::ACTION_BLANK;

  case PLAYER_NEXT: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Next
    dbus_respond_ok(m);
    return KeyConfig::ACTION_NEXT_CHAPTER;

  case PLAYER_PREVIOUS: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Previous
    dbus_respond_ok(m);
    return KeyConfig::ACTION_PREVIOUS_CHAPTER;

  case PLAYER_PAUSE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Pause
    dbus_respond_ok(m);
    return KeyConfig::ACTION_PAUSE;

  case PLAYER_PLAY: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Play
    dbus_respond_ok(m);
    return KeyConfig::ACTION_PLAY;

  case PLAYER_PLAY_PAUSE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - PlayPause
    dbus_respond_ok(m);
    return KeyConfig::ACTION_PLAYPAUSE;

  case PLAYER_STOP: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Stop
    dbus_respond_ok(m);
    return KeyConfig::ACTION_EXIT;

  case PLAYER_SEEK: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Seek
    {
      DBusError error;
      dbus_error_init(&error);

      int64_t offset;
      dbus_message_get_args(m, &error, DBUS_TYPE_INT64, &offset, DBUS_TYPE_INVALID);

      // Make sure a value is sent for seeking
      if (dbus_error_is_set(&error))
      {
         CLogLog(LOGWARNING, "Seek D-Bus Error: %s", error.message );
         dbus_error_free(&error);
         dbus_respond_ok(m);
         return KeyConfig::ACTION_BLANK;
      }
      else
      {
         dbus_respond_int64(m, offset);
         return OMXControlResult(KeyConfig::ACTION_SEEK_RELATIVE, offset);
      }
    }
  case PLAYER_SET_POSITION: // OMXPLAYER_DBUS_INTERFACE_PLAYER - SetPosition
    {
      DBusError error;
      dbus_error_init(&error);

      int64_t position;
      const char *oPath; // ignoring path right now because we don't have a playlist
      dbus_message_get_args(m, &error, DBUS_TYPE_OBJECT_PATH, &oPath, DBUS_TYPE_INT64, &position, DBUS_TYPE_INVALID);

      // Make sure a value is sent for setting position
      if (dbus_error_is_set(&error))
      {
        CLogLog(LOGWARNING, "SetPosition D-Bus Error: %s", error.message );
        dbus_error_free(&error);
        dbus_respond_ok(m);
        return KeyConfig::ACTION_BLANK;
      }
      else
      {
        dbus_respond_int64(m, position);
        return OMXControlResult(KeyConfig::ACTION_SEEK_ABSOLUTE, position);
      }
    }
  case PLAYER_SET_ALPHA: // OMXPLAYER_DBUS_INTERFACE_PLAYER - SetAlpha
    {
      DBusError error;
      dbus_error_init(&error);

      int64_t alpha;
      const char *oPath; // ignoring path right now because we don't have a playlist
      dbus_message_get_args(m, &error, DBUS_TYPE_OBJECT_PATH, &oPath, DBUS_TYPE_INT64, &alpha, DBUS_TYPE_INVALID);

      // Make sure a value is sent for setting alpha
      if (dbus_error_is_set(&error))
      {
        CLogLog(LOGWARNING, "SetAlpha D-Bus Error: %s", error.message );
        dbus_error_free(&error);
        dbus_respond_ok(m);
        return KeyConfig::ACTION_BLANK;
      }
      else
      {
        dbus_respond_int64(m, alpha);
        return OMXControlResult(KeyConfig::ACTION_SET_ALPHA, alpha);
      }
    }
  case PLAYER_SET_LAYER: // OMXPLAYER_DBUS_INTERFACE_PLAYER - SetLayer
    {
      DBusError error;
      dbus_error_init(&error);

      int64_t layer;
      dbus_message_get_args(m, &error, DBUS_TYPE_INT64, &layer, DBUS_TYPE_INVALID);

      // Make sure a value is sent for setting layer
      if (dbus_error_is_set(&error))
      {
        CLogLog(LOGWARNING, "SetLayer D-Bus Error: %s", error.message );
        dbus_error_free(&error);
        dbus_respond_ok(m);
        return KeyConfig::ACTION_BLANK;
      }
      else
      {
        dbus_respond_int64(m, layer);
        return OMXControlResult(KeyConfig::ACTION_SET_LAYER, layer);
      }
    }
  case PLAYER_SET_ASPECT_MODE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - SetAspectMode
    {
      DBusError error;
      dbus_error_init(&error);

      const char *aspectMode;
      const char *oPath; // ignoring path right now because we don't have a playlist
      dbus_message_get_args(m, &error, DBUS_TYPE_OBJECT_PATH, &oPath, DBUS_TYPE_STRING, &aspectMode, DBUS_TYPE_INVALID);

      // Make sure a value is sent for setting aspect mode
      if (dbus_error_is_set(&error))
      {
        CLogLog(LOGWARNING, "SetAspectMode D-Bus Error: %s", error.message );
        dbus_error_free(&error);
        dbus_respond_ok(m);
        return KeyConfig::ACTION_BLANK;
      }
      else
      {
        dbus_respond_string(m, aspectMode);
        return OMXControlResult(KeyConfig::ACTION_SET_ASPECT_MODE, aspectMode);
      }
    }
  case PLAYER_MUTE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Mute
    if(audio)
      audio->SetMute(true);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_BLANK;

  case PLAYER_UNMUTE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Unmute
    if(audio)
      audio->SetMute(false);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_BLANK;

  case PLAYER_LIST_SUBTITLES: // OMXPLAYER_DBUS_INTERFACE_PLAYER - ListSubtitles
    {
      int count = reader->SubtitleStreamCount();
      const char **values = new const char*[count];
      char *data = new char[30 * count];
      char *p = &data[0];
      char *end = &data[(30 * count) - 1];

      for (int i = 0; i < count && p < end; i++)
      {
         values[i] = p;
         p += 1 + snprintf(p, end - p, "%d:%s:%s", i,
                           reader->GetStreamMetaData(OMXSTREAM_SUBTITLE, i).c_str(),
                           subtitles->GetActiveStream() == i ? "active" : "");
      }

      dbus_respond_array(m, values, count);

      delete[] values;
      delete[] data;

      return KeyConfig::ACTION_BLANK;
    }
  case PLAYER_HIDE_VIDEO: // OMXPLAYER_DBUS_INTERFACE_PLAYER - HideVideo
    dbus_respond_ok(m);
    return KeyConfig::ACTION_HIDE_VIDEO;

  case PLAYER_UN_HIDE_VIDEO: // OMXPLAYER_DBUS_INTERFACE_PLAYER - UnHideVideo
    dbus_respond_ok(m);
    return KeyConfig::ACTION_UNHIDE_VIDEO;

  case PLAYER_LIST_AUDIO: // OMXPLAYER_DBUS_INTERFACE_PLAYER - ListAudio
    {
      int count = reader->AudioStreamCount();
      const char **values = new const char*[count];
      char *data = new char[30 * count];
      char *p = &data[0];
      char *end = &data[(30 * count) - 1];

      int active_stream = audio ? audio->GetActiveStream() : -1;

      for (int i = 0; i < count && p < end; i++)
      {
         values[i] = p;
         p += 1 + snprintf(p, end - p, "%d:%s:%s", i,
                           reader->GetStreamMetaData(OMXSTREAM_AUDIO, i).c_str(),
                           i == active_stream ? "active" : "");
      }

      dbus_respond_array(m, values, count);

      delete[] values;
      delete[] data;

      return KeyConfig::ACTION_BLANK;
    }
  case PLAYER_LIST_VIDEO: // OMXPLAYER_DBUS_INTERFACE_PLAYER - ListVideo
    {
      int count = reader->VideoStreamCount();
      const char **values = new const char*[count];
      char *data = new char[30 * count];
      char *p = &data[0];
      char *end = &data[(30 * count) - 1];

      for (int i = 0; i < count && p < end; i++)
      {
         values[i] = p;
         p += 1 + snprintf(p, end - p, "%d:%s:%s", i,
                           reader->GetStreamMetaData(OMXSTREAM_VIDEO, i).c_str(),
                           i == 0 ? "active" : "");
      }

      dbus_respond_array(m, values, count);

      delete[] values;
      delete[] data;

      return KeyConfig::ACTION_BLANK;
    }
  case PLAYER_SELECT_SUBTITLE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - SelectSubtitle
    {
      int index;
      if(!dbus_message_get_arg(m, DBUS_TYPE_INT32, &index))
      {
        dbus_respond_boolean(m, 0);
      }
      else
      {
        dbus_respond_boolean(m, subtitles->SetActiveStream(index) == index ? 1 : 0);
      }
      return KeyConfig::ACTION_BLANK;
    }
  case PLAYER_SELECT_AUDIO: // OMXPLAYER_DBUS_INTERFACE_PLAYER - SelectAudio
    {
      if(!audio)
        dbus_respond_boolean(m, 0);

      int index;
      if (!dbus_message_get_arg(m, DBUS_TYPE_INT32, &index))
      {
        dbus_respond_boolean(m, 0);
      }
      else
      {
        dbus_respond_boolean(m, audio->SetActiveStream(index) ? 1 : 0);
      }
      return KeyConfig::ACTION_BLANK;
    }
  // TODO: SelectVideo ???
  case PLAYER_SHOW_SUBTITLES: // OMXPLAYER_DBUS_INTERFACE_PLAYER - ShowSubtitles
    subtitles->SetVisible(true);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_SHOW_SUBTITLES;

  case PLAYER_HIDE_SUBTITLES: // OMXPLAYER_DBUS_INTERFACE_PLAYER - HideSubtitles
    subtitles->SetVisible(false);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_HIDE_SUBTITLES;

  case PLAYER_OPEN_URI: // OMXPLAYER_DBUS_INTERFACE_PLAYER - OpenUri
    {
      DBusError error;
      dbus_error_init(&error);

      const char *file;
      dbus_message_get_args(m, &error, DBUS_TYPE_STRING, &file, DBUS_TYPE_INVALID);

      if (dbus_error_is_set(&error))
      {
        CLogLog(LOGWARNING, "Change file D-Bus Error: %s", error.message );
        dbus_error_free(&error);
        dbus_respond_ok(m);
        return KeyConfig::ACTION_BLANK;
      }
      else
      {
        dbus_respond_string(m, file);
        return OMXControlResult(KeyConfig::ACTION_CHANGE_FILE, file);
      }
    }
  case PLAYER_ACTION: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Action
    {
      int action;
      if(!dbus_message_get_arg(m, DBUS_TYPE_INT32, &action))
        action = KeyConfig::ACTION_BLANK;

      dbus_respond_ok(m);
      return action; // Directly return enum
    }
  // begin get properties
  //Root interface:

  case GET_ROOT_CAN_RAISE: // OMXPLAYER_DBUS_INTERFACE_ROOT - CanRaise
    dbus_respond_boolean(m, 0);
    return KeyConfig::ACTION_BLANK;

  case GET_ROOT_CAN_SET_FULLSCREEN: // OMXPLAYER_DBUS_INTERFACE_ROOT - CanSetFullscreen
    dbus_respond_boolean(m, 0);
    return KeyConfig::ACTION_BLANK;

  case GET_ROOT_FULLSCREEN: // OMXPLAYER_DBUS_INTERFACE_ROOT - Fullscreen //Fullscreen is read/write in theory not read only, but read only at the moment so...
    dbus_respond_boolean(m, 1);
    return KeyConfig::ACTION_BLANK;

  case GET_ROOT_HAS_TRACK_LIST: // OMXPLAYER_DBUS_INTERFACE_ROOT - HasTrackList
    dbus_respond_boolean(m, 0);
    return KeyConfig::ACTION_BLANK;

  //Player interface:
  //MPRIS2 properties:

  case GET_PLAYER_MINIMUM_RATE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - MinimumRate
    dbus_respond_double(m, (MIN_RATE)/1000.);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_MAXIMUM_RATE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - MaximumRate
    dbus_respond_double(m, (MAX_RATE)/1000.);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_RATE: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Rate
    //return current playing rate
    dbus_respond_double(m, (double)clock->OMXPlaySpeed()/1000.);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_VOLUME: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Volume
    //return current volume
    dbus_respond_double(m, audio ? audio->GetVolume() : 0.0);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_METADATA: // OMXPLAYER_DBUS_INTERFACE_PLAYER - Metadata
    {
      DBusMessage *reply;
      reply = dbus_message_new_method_return(m);
      if(reply)
      {
        //Create iterator: Array of dict entries, composed of string (key)) and variant (value)
        DBusMessageIter array_cont, dict_cont, dict_entry_cont, var;
        dbus_message_iter_init_append(reply, &array_cont);
        dbus_message_iter_open_container(&array_cont, DBUS_TYPE_ARRAY, "{sv}", &dict_cont);
          //First dict entry: URI
          const char *key1 = "xesam:url";
          char uri[PATH_MAX+7];
          ToURI(get_filename(), uri);
          const char *value1=uri;
          dbus_message_iter_open_container(&dict_cont, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_cont);
            dbus_message_iter_append_basic(&dict_entry_cont, DBUS_TYPE_STRING, &key1);
            dbus_message_iter_open_container(&dict_entry_cont, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &var);
            dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &value1);
            dbus_message_iter_close_container(&dict_entry_cont, &var);
          dbus_message_iter_close_container(&dict_cont, &dict_entry_cont);
          //Second dict entry: duration in us
          const char *key2 = "mpris:length";
          dbus_int64_t value2 = reader->GetStreamLengthMicro();
          dbus_message_iter_open_container(&dict_cont, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_cont);
            dbus_message_iter_append_basic(&dict_entry_cont, DBUS_TYPE_STRING, &key2);
            dbus_message_iter_open_container(&dict_entry_cont, DBUS_TYPE_VARIANT, DBUS_TYPE_INT64_AS_STRING, &var);
            dbus_message_iter_append_basic(&var, DBUS_TYPE_INT64, &value2);
            dbus_message_iter_close_container(&dict_entry_cont, &var);
          dbus_message_iter_close_container(&dict_cont, &dict_entry_cont);
        dbus_message_iter_close_container(&array_cont, &dict_cont);
        //Send message
        dbus_connection_send(bus, reply, NULL);
        dbus_message_unref(reply);
      }
      return KeyConfig::ACTION_BLANK;
    }

  //Non-MPRIS2 properties:

  // end get properties
  }

  // this should never happen!
  return KeyConfig::ACTION_BLANK;
}

OMXControlResult OMXControl::SetProperty(DBusMessage *m)
{
  DBusError error;
  dbus_error_init(&error);

  //Retrieve interface, property name and value
  //Message has the form message[STRING:interface STRING:property DOUBLE:value] or message[STRING:interface STRING:property VARIANT[DOUBLE:value]]
  const char *interface, *property;
  double new_property_value;
  DBusMessageIter args;
  dbus_message_iter_init(m, &args);
  if(dbus_message_iter_has_next(&args))
  {
    //The interface name
    if( DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&args) )
      dbus_message_iter_get_basic (&args, &interface);
    else
    {
      CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
      dbus_error_free(&error);
      dbus_respond_error(m, DBUS_ERROR_INVALID_ARGS, "Invalid arguments");
      return KeyConfig::ACTION_BLANK;
    }
    //The property name
    if( dbus_message_iter_next(&args) && DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&args) )
      dbus_message_iter_get_basic (&args, &property);
    else
    {
      CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
      dbus_error_free(&error);
      dbus_respond_error(m, DBUS_ERROR_INVALID_ARGS, "Invalid arguments");
      return KeyConfig::ACTION_BLANK;
    }
    //The value (either double or double in variant)
    if (dbus_message_iter_next(&args))
    {
      //Simply a double
      if (DBUS_TYPE_DOUBLE == dbus_message_iter_get_arg_type(&args))
      {
        dbus_message_iter_get_basic(&args, &new_property_value);
      }
      //A double within a variant
      else if(DBUS_TYPE_VARIANT == dbus_message_iter_get_arg_type(&args))
      {
        DBusMessageIter variant;
        dbus_message_iter_recurse(&args, &variant);
        if(DBUS_TYPE_DOUBLE == dbus_message_iter_get_arg_type(&variant))
        {
          dbus_message_iter_get_basic(&variant, &new_property_value);
        }
      }
      else
      {
        CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
        dbus_error_free(&error);
        dbus_respond_error(m, DBUS_ERROR_INVALID_ARGS, "Invalid arguments");
        return KeyConfig::ACTION_BLANK;
      }
    }
  }
  if ( dbus_error_is_set(&error) )
  {
      CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
      dbus_error_free(&error);
      dbus_respond_error(m, DBUS_ERROR_INVALID_ARGS, "Invalid arguments");
      return KeyConfig::ACTION_BLANK;
  }
  //Player interface:
  if (strcmp(interface, OMXPLAYER_DBUS_INTERFACE_PLAYER)==0)
  {
    if (strcmp(property, "Volume")==0)
    {
      double volume=new_property_value;
      //Min value is 0
      if(volume<.0)
      {
        volume=.0;
      }

      if(audio)
        audio->SetVolume(volume);
      else
        volume = 0.0;

      dbus_respond_double(m, volume);
      return KeyConfig::ACTION_BLANK;
    }
    else if (strcmp(property, "Rate")==0)
    {
      double rate=new_property_value;
      if(rate>MAX_RATE/1000.)
      {
        rate=MAX_RATE/1000.;
      }
      if(rate<MIN_RATE/1000.)
      {
        //Set to Pause according to MPRIS2 specs (no actual change of playing rate)
        dbus_respond_double(m, (double)clock->OMXPlaySpeed()/1000.);
        return KeyConfig::ACTION_PAUSE;
      }
      int iSpeed=(int)(rate*1000.);
      if(!clock)
      {
        dbus_respond_double(m, .0);//What value ????
        return KeyConfig::ACTION_BLANK;
      }
      //Can't do trickplay here so limit max speed
      if(iSpeed > MAX_RATE)
        iSpeed=MAX_RATE;
      dbus_respond_double(m, iSpeed/1000.);//Reply before applying to be faster
      clock->OMXSetSpeed(iSpeed, false, true);
      return KeyConfig::ACTION_PLAY;
    }
    //Wrong property
    else
    {
      //Error
      CLogLog(LOGWARNING, "Unhandled dbus property message, member: %s interface: %s type: %d path: %s  property: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m), property );
      dbus_respond_error(m, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
      return KeyConfig::ACTION_BLANK;
    }
  }
  //Wrong interface:
  else
  {
      //Error
      CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
      dbus_respond_error(m, DBUS_ERROR_UNKNOWN_INTERFACE, "Unknown interface");
      return KeyConfig::ACTION_BLANK;
  }
}

DBusHandlerResult OMXControl::dbus_respond_error(DBusMessage *m, const char *name, const char *msg)
{
  DBusMessage *reply;

  reply = dbus_message_new_error(m, name, msg);

  if (!reply)
    return DBUS_HANDLER_RESULT_NEED_MEMORY;

  dbus_connection_send(bus, reply, NULL);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult OMXControl::dbus_respond_ok(DBusMessage *m)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(m);

  if (!reply)
    return DBUS_HANDLER_RESULT_NEED_MEMORY;

  dbus_connection_send(bus, reply, NULL);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult OMXControl::dbus_respond_string(DBusMessage *m, const char *text)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(m);

  if (!reply)
  {
    CLogLog(LOGWARNING, "Failed to allocate message");
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  }

  dbus_message_append_args(reply, DBUS_TYPE_STRING, &text, DBUS_TYPE_INVALID);
  dbus_connection_send(bus, reply, NULL);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult OMXControl::dbus_respond_int64(DBusMessage *m, int64_t i)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(m);

  if (!reply)
  {
    CLogLog(LOGWARNING, "Failed to allocate message");
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  }

  dbus_message_append_args(reply, DBUS_TYPE_INT64, &i, DBUS_TYPE_INVALID);
  dbus_connection_send(bus, reply, NULL);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult OMXControl::dbus_respond_double(DBusMessage *m, double d)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(m);

  if (!reply) 
  {
    CLogLog(LOGWARNING, "Failed to allocate message");
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  }

  dbus_message_append_args(reply, DBUS_TYPE_DOUBLE, &d, DBUS_TYPE_INVALID);
  dbus_connection_send(bus, reply, NULL);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult OMXControl::dbus_respond_boolean(DBusMessage *m, int b)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(m);

  if (!reply)
  {
    CLogLog(LOGWARNING, "Failed to allocate message");
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  }

  dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &b, DBUS_TYPE_INVALID);
  dbus_connection_send(bus, reply, NULL);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult OMXControl::dbus_respond_array(DBusMessage *m, const char *array[], int size)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(m);

  if (!reply)
  {
    CLogLog(LOGWARNING, "Failed to allocate message");
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  }

  dbus_message_append_args(reply, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, size, DBUS_TYPE_INVALID);
  dbus_connection_send(bus, reply, NULL);
  dbus_message_unref(reply);

  return DBUS_HANDLER_RESULT_HANDLED;
}
