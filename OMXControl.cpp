#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>

#include <string>

#include "utils/log.h"
#include "OMXClock.h"
#include "OMXControl.h"
#include "KeyConfig.h"
#include "OMXPlayerAudio.h"
#include "OMXReader.h"
#include "OMXPlayerSubtitles.h"
#include "utils/misc.h"

#include "DbusCommandSearch.h"

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
  v.int64arg = newArg;
}

OMXControlResult::OMXControlResult( int newKey, double newArg ) {
  key = newKey;
  v.doublearg = newArg;
}

OMXControlResult::OMXControlResult( int newKey, const char *newArg ) {
  key = newKey;
  v.strarg = newArg;
}

int OMXControlResult::getKey() {
  return key;
}

int64_t OMXControlResult::getArg() {
  return v.int64arg;
}

double OMXControlResult::getDoubleArg() {
  return v.doublearg;
}

const char *OMXControlResult::getStrArg() {
  return v.strarg;
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

  const char *method = dbus_message_get_member(m);

  OMXControlResult action = handle_event(m, dbus_find_method(method));

  dbus_message_unref(m);

  return action;
}

OMXControlResult OMXControl::handle_event(DBusMessage *m, enum DBusMethod search_key)
{
  switch(search_key)
  {
  case INVALID_METHOD:
    CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
    dbus_respond_error(m, DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
    return KeyConfig::ACTION_BLANK;

  case INVALID_PROPERTY:
    CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
    dbus_respond_error(m, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
    return KeyConfig::ACTION_BLANK;

  case QUIT:
    dbus_respond_ok(m);//Note: No reply according to MPRIS2 specs
    return KeyConfig::ACTION_EXIT;

  case RAISE:
    //Does nothing
    return KeyConfig::ACTION_BLANK;

  case GET:
    {
      DBusError error;
      dbus_error_init(&error);

      //Retrieve interface and property name
      const char *interface, *property;
      dbus_message_get_args(m, &error, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID);

      if (dbus_error_is_set(&error))
      {
         CLogLog(LOGWARNING, "Seek D-Bus Error: %s", error.message);
         dbus_error_free(&error);
         dbus_respond_ok(m);
         return KeyConfig::ACTION_BLANK;
      }
      else
      {
        return handle_event(m, dbus_find_property(property));
      }
    }

  case SET:
    return SetProperty(m);

  case CAN_QUIT:
  case FULLSCREEN:
  case CAN_CONTROL:
  case CAN_PLAY:
  case CAN_PAUSE:
  case GET_ROOT_FULLSCREEN:
    dbus_respond_boolean(m, 1);
    return KeyConfig::ACTION_BLANK;

  case CAN_SET_FULLSCREEN:
  case CAN_RAISE:
  case HAS_TRACK_LIST:
  case CAN_GO_NEXT:
  case CAN_GO_PREVIOUS:
  case GET_ROOT_CAN_RAISE:
  case GET_ROOT_HAS_TRACK_LIST:
    dbus_respond_boolean(m, 0);
    return KeyConfig::ACTION_BLANK;

  case IDENTITY:
    dbus_respond_string(m, "OMXPlayer");
    return KeyConfig::ACTION_BLANK;

  case SUPPORTED_URI_SCHEMES:
    {
      const char *UriSchemes[] = {"file", "http", "rtsp", "rtmp"};
      dbus_respond_array(m, UriSchemes, 4);
      return KeyConfig::ACTION_BLANK;
    }

  case SUPPORTED_MIME_TYPES:
    {
      const char *MimeTypes[] = {}; // Needs supplying
      dbus_respond_array(m, MimeTypes, 0);
      return KeyConfig::ACTION_BLANK;
    }

  case CAN_SEEK:
    dbus_respond_boolean(m, reader->CanSeek());
    return KeyConfig::ACTION_BLANK;

  case PLAYBACK_STATUS:
    dbus_respond_string(m, clock->OMXIsPaused() ? "Paused" : "Playing");
    return KeyConfig::ACTION_BLANK;

  case GET_SOURCE:
    dbus_respond_string(m, get_filename().c_str());
    return KeyConfig::ACTION_BLANK;

  case VOLUME:
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

        dbus_respond_double(m, volume);
        return OMXControlResult(KeyConfig::SET_VOLUME, volume);
      }
    }

  case MUTE:
    if(audio)
      audio->SetMute(true);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_BLANK;

  case UNMUTE:
    if(audio)
      audio->SetMute(false);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_BLANK;

  case POSITION:
    // Returns the current position in microseconds
    dbus_respond_int64(m, clock->OMXMediaTime());
    return KeyConfig::ACTION_BLANK;

  case ASPECT:
    // Returns aspect ratio
    dbus_respond_double(m, reader->GetAspectRatio());
    return KeyConfig::ACTION_BLANK;

  case VIDEO_STREAM_COUNT:
    // Returns number of video streams
    dbus_respond_int64(m, reader->VideoStreamCount());
    return KeyConfig::ACTION_BLANK;

  case RES_WIDTH:
    // Returns width of video
    dbus_respond_int64(m, reader->GetWidth());
    return KeyConfig::ACTION_BLANK;

  case RES_HEIGHT:
    // Returns height of video
    dbus_respond_int64(m, reader->GetHeight());
    return KeyConfig::ACTION_BLANK;

  case DURATION:
    // Returns the duration in microseconds
    dbus_respond_int64(m, reader->GetStreamLengthMicro());
    return KeyConfig::ACTION_BLANK;

  case MINIMUM_RATE:
    dbus_respond_double(m, 0.0);
    return KeyConfig::ACTION_BLANK;

  case MAXIMUM_RATE:
    //TODO: to be made consistent
    dbus_respond_double(m, 10.125);
    return KeyConfig::ACTION_BLANK;

  case NEXT:
    dbus_respond_ok(m);
    return KeyConfig::ACTION_NEXT_CHAPTER;

  case PREVIOUS:
    dbus_respond_ok(m);
    return KeyConfig::ACTION_PREVIOUS_CHAPTER;

  case PAUSE:
    dbus_respond_ok(m);
    return KeyConfig::ACTION_PAUSE;

  case PLAY:
    dbus_respond_ok(m);
    return KeyConfig::ACTION_PLAY;

  case PLAY_PAUSE:
    dbus_respond_ok(m);
    return KeyConfig::ACTION_PLAYPAUSE;

  case STOP:
    dbus_respond_ok(m);
    return KeyConfig::ACTION_EXIT;

  case SEEK:
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

  case SET_POSITION:
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

  case SET_ALPHA:
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

  case SET_LAYER:
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

  case SET_ASPECT_MODE:
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

  case LIST_SUBTITLES:
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

  case HIDE_VIDEO:
    dbus_respond_ok(m);
    return KeyConfig::ACTION_HIDE_VIDEO;

  case UN_HIDE_VIDEO:
    dbus_respond_ok(m);
    return KeyConfig::ACTION_UNHIDE_VIDEO;

  case LIST_AUDIO:
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

  case LIST_VIDEO:
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

  case SELECT_SUBTITLE:
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

  case SELECT_AUDIO:
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
  case SHOW_SUBTITLES:
    subtitles->SetVisible(true);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_SHOW_SUBTITLES;

  case HIDE_SUBTITLES:
    subtitles->SetVisible(false);
    dbus_respond_ok(m);
    return KeyConfig::ACTION_HIDE_SUBTITLES;

  case OPEN_URI:
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

  case ACTION:
    {
      int action;
      if(!dbus_message_get_arg(m, DBUS_TYPE_INT32, &action))
        action = KeyConfig::ACTION_BLANK;

      dbus_respond_ok(m);
      return action; // Directly return enum
    }

  case GET_PLAYER_MINIMUM_RATE:
    dbus_respond_double(m, (MIN_RATE)/1000.);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_MAXIMUM_RATE:
    dbus_respond_double(m, (MAX_RATE)/1000.);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_RATE:
    //return current playing rate
    dbus_respond_double(m, (double)clock->OMXPlaySpeed()/1000.);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_VOLUME:
    //return current volume
    dbus_respond_double(m, audio ? audio->GetVolume() : 0.0);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_METADATA:
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
  //Retrieve interface, property name and value
  //Message has the form message[STRING:interface STRING:property DOUBLE:value] or message[STRING:interface STRING:property VARIANT[DOUBLE:value]]
  const char *interface, *property;
  double new_property_value;
  DBusMessageIter args;
  dbus_message_iter_init(m, &args);
  if(!dbus_message_iter_has_next(&args))
    goto invalid_argument;
  else
  {
    //The interface name
    if( DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&args) )
      dbus_message_iter_get_basic (&args, &interface);
    else
      goto invalid_argument;

    //The property name
    if( dbus_message_iter_next(&args) && DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&args) )
      dbus_message_iter_get_basic (&args, &property);
    else
      goto invalid_argument;

    //The value (either double or double in variant)
    if (dbus_message_iter_next(&args))
    {
      //Simply a double
      if (DBUS_TYPE_DOUBLE == dbus_message_iter_get_arg_type(&args))
        dbus_message_iter_get_basic(&args, &new_property_value);

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
        goto invalid_argument;
    }
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
        volume = 0.0;
      }

      dbus_respond_double(m, volume);
      return OMXControlResult(KeyConfig::SET_VOLUME, volume);
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


invalid_argument:
  CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
  dbus_respond_error(m, DBUS_ERROR_INVALID_ARGS, "Invalid arguments");
  return KeyConfig::ACTION_BLANK;
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
