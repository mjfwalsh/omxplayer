#include <stdio.h>
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

OMXControlResult::OMXControlResult( int newKey ) {
  key = newKey;
}

OMXControlResult::OMXControlResult( int newKey, int newArg ) {
  key = newKey;
  v.intarg = newArg;
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

int OMXControlResult::getIntArg() {
  return v.intarg;
}

int64_t OMXControlResult::getInt64Arg() {
  return v.int64arg;
}

double OMXControlResult::getDoubleArg() {
  return v.doublearg;
}

const char *OMXControlResult::getStrArg() {
  return v.strarg;
}

class DMessage
{
private:
  DBusMessage *m;
  DBusConnection *m_bus;
  DBusMessageIter *m_args = NULL;

public:
  DMessage(DBusMessage *message, DBusConnection *bus);
  ~DMessage();

  operator DBusMessage*() const { return m; }

  template <class T> bool get_arg(int type, T *value);
  bool ignore_arg();
  void respond_error(const char *name, const char *msg);
  void respond_ok();
  template <class T> void respond(int type, T value);
  void respond_array(const char *array[], int size);
};


#define CLASSNAME "OMXControl"

OMXControl::~OMXControl() 
{
  dbus_disconnect();
}

void OMXControl::init(OMXClock *av_clock, OMXPlayerSubtitles *player_subtitles,
    std::string& (*filename)(), int (*speed)(double &s))
{
  clock        = av_clock;
  subtitles    = player_subtitles;
  get_filename = filename;
  get_approx_speed = speed;
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

  DMessage message(m, bus);

  OMXControlResult action = handle_event(message, dbus_find_method(method));

  return action;
}


OMXControlResult OMXControl::handle_event(DMessage &m, enum DBusMethod search_key)
{
  switch(search_key)
  {
  case INVALID_METHOD:
    CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
    m.respond_error(DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
    return KeyConfig::ACTION_BLANK;

  case INVALID_PROPERTY:
    CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
    m.respond_error(DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
    return KeyConfig::ACTION_BLANK;

  case QUIT:
    m.respond_ok();//Note: No reply according to MPRIS2 specs
    return KeyConfig::ACTION_EXIT;

  case RAISE:
    //Does nothing
    return KeyConfig::ACTION_BLANK;

  case GET:
    {
      //Retrieve interface and property name
      const char *property;
      if(m.ignore_arg() && m.get_arg(DBUS_TYPE_STRING, &property))
      {
        return handle_event(m, dbus_find_property(property));
      }
      else
      {
         CLogLog(LOGWARNING, "Seek D-Bus Error: Bad params");
         m.respond_ok();
         return KeyConfig::ACTION_BLANK;
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
    m.respond(DBUS_TYPE_BOOLEAN, 1);
    return KeyConfig::ACTION_BLANK;

  case CAN_SET_FULLSCREEN:
  case CAN_RAISE:
  case HAS_TRACK_LIST:
  case CAN_GO_NEXT:
  case CAN_GO_PREVIOUS:
  case GET_ROOT_CAN_RAISE:
  case GET_ROOT_HAS_TRACK_LIST:
    m.respond(DBUS_TYPE_BOOLEAN, 0);
    return KeyConfig::ACTION_BLANK;

  case IDENTITY:
    m.respond(DBUS_TYPE_STRING, "OMXPlayer");
    return KeyConfig::ACTION_BLANK;

  case SUPPORTED_URI_SCHEMES:
    {
      const char *UriSchemes[] = {"file", "http", "rtsp", "rtmp"};
      m.respond_array(UriSchemes, 4);
      return KeyConfig::ACTION_BLANK;
    }

  case SUPPORTED_MIME_TYPES:
    {
      const char *MimeTypes[] = {}; // Needs supplying
      m.respond_array(MimeTypes, 0);
      return KeyConfig::ACTION_BLANK;
    }

  case CAN_SEEK:
    m.respond(DBUS_TYPE_BOOLEAN, reader->CanSeek());
    return KeyConfig::ACTION_BLANK;

  case PLAYBACK_STATUS:
    m.respond(DBUS_TYPE_STRING, clock->OMXIsPaused() ? "Paused" : "Playing");
    return KeyConfig::ACTION_BLANK;

  case GET_SOURCE:
    m.respond(DBUS_TYPE_STRING, get_filename().c_str());
    return KeyConfig::ACTION_BLANK;

  case VOLUME:
    {
      if(!audio)
      {
        m.respond(DBUS_TYPE_DOUBLE, 0.0);
        return KeyConfig::ACTION_BLANK;
      }

      double volume;
      if(m.get_arg(DBUS_TYPE_DOUBLE, &volume))
      {
        if(volume < 0.0) volume = 0.0;
        m.respond(DBUS_TYPE_DOUBLE, volume);
        return OMXControlResult(KeyConfig::ACTION_SET_VOLUME, volume);
      }
      else
      {
        m.respond(DBUS_TYPE_DOUBLE, audio->GetVolume());
        return KeyConfig::ACTION_BLANK;
      }
    }

  case MUTE:
    if(audio)
      audio->SetMute(true);
    m.respond_ok();
    return KeyConfig::ACTION_BLANK;

  case UNMUTE:
    if(audio)
      audio->SetMute(false);
    m.respond_ok();
    return KeyConfig::ACTION_BLANK;

  case POSITION:
    // Returns the current position in microseconds
    m.respond(DBUS_TYPE_INT64, clock->OMXMediaTime());
    return KeyConfig::ACTION_BLANK;

  case ASPECT:
    // Returns aspect ratio
    m.respond(DBUS_TYPE_DOUBLE, reader->GetAspectRatio());
    return KeyConfig::ACTION_BLANK;

  case VIDEO_STREAM_COUNT:
    // Returns number of video streams
    m.respond(DBUS_TYPE_INT64, reader->VideoStreamCount());
    return KeyConfig::ACTION_BLANK;

  case RES_WIDTH:
    // Returns width of video
    m.respond(DBUS_TYPE_INT64, reader->GetWidth());
    return KeyConfig::ACTION_BLANK;

  case RES_HEIGHT:
    // Returns height of video
    m.respond(DBUS_TYPE_INT64, reader->GetHeight());
    return KeyConfig::ACTION_BLANK;

  case DURATION:
    // Returns the duration in microseconds
    m.respond(DBUS_TYPE_INT64, reader->GetStreamLengthMicro());
    return KeyConfig::ACTION_BLANK;

  case MINIMUM_RATE:
    m.respond(DBUS_TYPE_DOUBLE, 0.0);
    return KeyConfig::ACTION_BLANK;

  case MAXIMUM_RATE:
    //TODO: to be made consistent
    m.respond(DBUS_TYPE_DOUBLE, 4.0);
    return KeyConfig::ACTION_BLANK;

  case NEXT:
    m.respond_ok();
    return KeyConfig::ACTION_NEXT_CHAPTER;

  case PREVIOUS:
    m.respond_ok();
    return KeyConfig::ACTION_PREVIOUS_CHAPTER;

  case PAUSE:
    m.respond_ok();
    return KeyConfig::ACTION_PAUSE;

  case PLAY:
    m.respond_ok();
    return KeyConfig::ACTION_PLAY;

  case PLAY_PAUSE:
    m.respond_ok();
    return KeyConfig::ACTION_PLAYPAUSE;

  case STOP:
    m.respond_ok();
    return KeyConfig::ACTION_EXIT;

  case SEEK:
    {
      int64_t offset;
      if(!m.get_arg(DBUS_TYPE_INT64, &offset))
      {
         CLogLog(LOGWARNING, "Seek D-Bus Error: Bad params");
         m.respond_ok();
         return KeyConfig::ACTION_BLANK;
      }
      else
      {
         m.respond(DBUS_TYPE_INT64, offset);
         return OMXControlResult(KeyConfig::ACTION_SEEK_RELATIVE, offset);
      }
    }

  case SET_POSITION:
    {
      int64_t position;

      // Make sure a value is sent for setting position
      if(m.ignore_arg() && m.get_arg(DBUS_TYPE_INT64, &position))
      {
        m.respond(DBUS_TYPE_INT64, position);
        return OMXControlResult(KeyConfig::ACTION_SEEK_ABSOLUTE, position);
      }
      else
      {
        CLogLog(LOGWARNING, "SetPosition D-Bus Error: Bad params");
        m.respond_ok();
        return KeyConfig::ACTION_BLANK;
      }
    }

  case SET_ALPHA:
    {
      int64_t alpha;

      // Make sure a value is sent for setting alpha
      if(m.ignore_arg() && m.get_arg(DBUS_TYPE_INT64, &alpha))
      {
        m.respond(DBUS_TYPE_INT64, alpha);
        return OMXControlResult(KeyConfig::ACTION_SET_ALPHA, alpha);
      }
      else
      {
        CLogLog(LOGWARNING, "SetPosition D-Bus Error: Bad params");
        m.respond_ok();
        return KeyConfig::ACTION_BLANK;
      }
    }

  case SET_LAYER:
    {
      int64_t layer;

      // Make sure a value is sent for setting layer
      if(m.ignore_arg() && m.get_arg(DBUS_TYPE_INT64, &layer))
      {
        m.respond(DBUS_TYPE_INT64, layer);
        return OMXControlResult(KeyConfig::ACTION_SET_LAYER, layer);
      }
      else
      {
        CLogLog(LOGWARNING, "SetPosition D-Bus Error: Bad params");
        m.respond_ok();
        return KeyConfig::ACTION_BLANK;
      }
    }

  case SET_ASPECT_MODE:
    {
      const char *aspectMode;

      // Make sure a value is sent for setting layer
      if(!m.ignore_arg() && m.get_arg(DBUS_TYPE_STRING, &aspectMode))
      {
        CLogLog(LOGWARNING, "SetPosition D-Bus Error: Bad params");
        m.respond_ok();
        return KeyConfig::ACTION_BLANK;
      }

      int aspect;
      if (!strcasecmp(aspectMode, "letterbox"))
        aspect = 1;
      else if (!strcasecmp(aspectMode, "fill"))
        aspect = 2;
      else if (!strcasecmp(aspectMode, "stretch"))
        aspect = 3;
      else
      {
        CLogLog(LOGWARNING, "SetPosition D-Bus Error: Bad params");
        m.respond_ok();
        return KeyConfig::ACTION_BLANK;
      }

      m.respond(DBUS_TYPE_STRING, aspectMode);
      return OMXControlResult(KeyConfig::ACTION_SET_ASPECT_MODE, aspect);
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

      m.respond_array(values, count);

      delete[] values;
      delete[] data;

      return KeyConfig::ACTION_BLANK;
    }

  case HIDE_VIDEO:
    m.respond_ok();
    return KeyConfig::ACTION_HIDE_VIDEO;

  case UN_HIDE_VIDEO:
    m.respond_ok();
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

      m.respond_array(values, count);

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

      m.respond_array(values, count);

      delete[] values;
      delete[] data;

      return KeyConfig::ACTION_BLANK;
    }

  case SELECT_SUBTITLE:
    {
      int index;
      if(!m.get_arg(DBUS_TYPE_INT32, &index))
      {
        m.respond(DBUS_TYPE_BOOLEAN, 0);
        return KeyConfig::ACTION_BLANK;
      }
      else
      {
        m.respond(DBUS_TYPE_BOOLEAN, subtitles->SetActiveStream(index) == index ? 1 : 0);
        return KeyConfig::ACTION_UPDATE_SUBTITLES;
      }
    }

  case SELECT_AUDIO:
    {
      if(!audio)
        m.respond(DBUS_TYPE_BOOLEAN, 0);

      int index;
      if (!m.get_arg(DBUS_TYPE_INT32, &index))
      {
        m.respond(DBUS_TYPE_BOOLEAN, 0);
        return KeyConfig::ACTION_BLANK;
      }
      else
      {
        m.respond(DBUS_TYPE_BOOLEAN, audio->SetActiveStream(index) ? 1 : 0);
        return KeyConfig::ACTION_UPDATE_AUDIO;
      }
    }

  case SHOW_SUBTITLES:
    m.respond_ok();
    return KeyConfig::ACTION_SHOW_SUBTITLES;

  case HIDE_SUBTITLES:
    m.respond_ok();
    return KeyConfig::ACTION_HIDE_SUBTITLES;

  case OPEN_URI:
    {
      const char *file;
      if (!m.get_arg(DBUS_TYPE_STRING, &file))
      {
        CLogLog(LOGWARNING, "Change file D-Bus Error: Bad Params");
        m.respond_ok();
        return KeyConfig::ACTION_BLANK;
      }
      else
      {
        m.respond(DBUS_TYPE_STRING, file);
        return OMXControlResult(KeyConfig::ACTION_CHANGE_FILE, file);
      }
    }

  case ACTION:
    {
      int action;
      if(!m.get_arg(DBUS_TYPE_INT32, &action) || action > KeyConfig::ACTION_BLANK)
        action = KeyConfig::ACTION_BLANK;

      m.respond_ok();
      return action; // Directly return enum
    }

  case GET_PLAYER_MINIMUM_RATE:
    m.respond(DBUS_TYPE_DOUBLE, (MIN_RATE)/1000.);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_MAXIMUM_RATE:
    m.respond(DBUS_TYPE_DOUBLE, (MAX_RATE)/1000.);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_RATE:
    //return current playing rate
    m.respond(DBUS_TYPE_BOOLEAN, (double)clock->OMXPlaySpeed()/1000.);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_VOLUME:
    //return current volume
    m.respond(DBUS_TYPE_BOOLEAN, audio ? audio->GetVolume() : 0.0);
    return KeyConfig::ACTION_BLANK;

  case GET_PLAYER_METADATA:
    {
      DBusMessage *reply = dbus_message_new_method_return(m); // dbus_message_new_return_method(m);
      if(reply)
      {
        //Create iterator: Array of dict entries, composed of string (key)) and variant (value)
        DBusMessageIter array_cont, dict_cont, dict_entry_cont, var;
        dbus_message_iter_init_append(reply, &array_cont);
        dbus_message_iter_open_container(&array_cont, DBUS_TYPE_ARRAY, "{sv}", &dict_cont);
          //First dict entry: URI
          const char *key1 = "xesam:url";
          std::string url = get_filename();
          if(!IsURL(url))
            url = "file://" + url;
          const char *value1 = url.c_str();

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
  }

  // this should never happen!
  return KeyConfig::ACTION_BLANK;
}


OMXControlResult OMXControl::SetProperty(DMessage &m)
{
  //Retrieve interface, property name and value
  //Message has the form message[STRING:interface STRING:property DOUBLE:value] or message[STRING:interface STRING:property VARIANT[DOUBLE:value]]
  char *property;

  if(!m.ignore_arg() || !m.get_arg(DBUS_TYPE_STRING, &property))
    goto invalid_args;

  if(strcmp(property, "Volume")==0)
  {
    return handle_event(m, VOLUME);
  }
  else if (strcmp(property, "Rate")==0)
  {
    double rate;
    if(!m.get_arg(DBUS_TYPE_DOUBLE, &rate))
      goto invalid_args;

    int new_speed = get_approx_speed(rate);
    m.respond(DBUS_TYPE_DOUBLE, rate);

    if(new_speed == 0)
      return KeyConfig::ACTION_PAUSE;
    else
      return OMXControlResult(KeyConfig::ACTION_SET_SPEED, new_speed);
  }

  //Wrong property
  CLogLog(LOGWARNING, "Unhandled dbus property message, member: %s interface: %s type: %d path: %s  property: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m), property );
  m.respond_error(DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
  return KeyConfig::ACTION_BLANK;

invalid_args:
  CLogLog(LOGWARNING, "Unhandled dbus message, member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );
  m.respond_error(DBUS_ERROR_INVALID_ARGS, "Invalid arguments");
  return KeyConfig::ACTION_BLANK;
}

DMessage::DMessage(DBusMessage *message, DBusConnection *bus)
{
  m = message;
  m_bus = bus;
}

DMessage::~DMessage()
{
  if(m_args)
    delete m_args;

  dbus_message_unref(m);
}

template <class T>
bool DMessage::get_arg(int type, T *value)
{
  if(!m_args)
  {
    m_args = new DBusMessageIter;
    dbus_message_iter_init(m, m_args);
  }

  int element_type = dbus_message_iter_get_arg_type(m_args);

  if(element_type == type)
  {
    dbus_message_iter_get_basic(m_args, value);
    dbus_message_iter_next(m_args);
    return true;
  }
  else if(element_type == DBUS_TYPE_VARIANT)
  {
    DBusMessageIter variant;
    dbus_message_iter_recurse(m_args, &variant);
    if(dbus_message_iter_get_arg_type(&variant) == type)
    {
      dbus_message_iter_get_basic(&variant, value);
    }
    dbus_message_iter_next(m_args);
    return true;
  }
  else
  {
    return false;
  }
}

bool DMessage::ignore_arg()
{
  if(!m_args)
  {
    m_args = new DBusMessageIter;
    dbus_message_iter_init(m, m_args);
  }

  return dbus_message_iter_next(m_args);
}

void DMessage::respond_error(const char *name, const char *msg)
{
  DBusMessage *reply;

  reply = dbus_message_new_error(m, name, msg);

  if(!reply)
    throw "memory error";

  dbus_connection_send(m_bus, reply, NULL);

  dbus_message_unref(reply);
}

void DMessage::respond_ok()
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(m);

  if(!reply)
    throw "memory error";

  dbus_connection_send(m_bus, reply, NULL);
  dbus_message_unref(reply);
}

template <class T>
void DMessage::respond(int type, T value)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(m);

  if (!reply)
    throw "Memory error";

  dbus_message_append_args(reply, type, &value, DBUS_TYPE_INVALID);
  dbus_connection_send(m_bus, reply, NULL);

  dbus_message_unref(reply);
}

void DMessage::respond_array(const char *array[], int size)
{
  DBusMessage *reply;

  reply = dbus_message_new_method_return(m);

  if (!reply)
    throw "Memory error";

  dbus_message_append_args(reply, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, size, DBUS_TYPE_INVALID);
  dbus_connection_send(m_bus, reply, NULL);
  dbus_message_unref(reply);
}
