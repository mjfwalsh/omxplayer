#include <dbus/dbus.h>
#include <vector>
#include <string>

#include "utils/log.h"
#include "OMXControl.h"
#include "KeyConfig.h"
#include "DbusCommandSearch.h"

static DBusConnection *bus = nullptr;

OMXControl::~OMXControl()
{
  dbus_disconnect();
}

bool OMXControl::connect(const char *dbus_name)
{
  if(dbus_connect(dbus_name))
  {
    CLogLog(LOGDEBUG, "DBus connection succeeded");
    dbus_threads_init_default();
    return true;
  }
  else
  {
    CLogLog(LOGWARNING, "DBus connection failed");
    dbus_disconnect();
    return false;
  }
}

void OMXControl::dispatch()
{
  if (bus)
    dbus_connection_read_write(bus, 0);
}


OMXControl::operator bool() const
{
  return bus;
}

bool OMXControl::dbus_connect(const char *dbus_name)
{
  if(bus != nullptr)
    throw "Only one dbus connection can exist";

  DBusError error;

  dbus_error_init(&error);
  if (!(bus = dbus_bus_get_private(DBUS_BUS_SESSION, &error)))
  {
    CLogLog(LOGWARNING, "dbus_bus_get_private(): %s", error.message);
      goto fail;
  }

  dbus_connection_set_exit_on_disconnect(bus, FALSE);

  if (dbus_bus_request_name(bus, dbus_name, DBUS_NAME_FLAG_DO_NOT_QUEUE, &error)
      != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
  {
    if (dbus_error_is_set(&error))
    {
      CLogLog(LOGWARNING, "dbus_bus_request_name(): %s", error.message);
      goto fail;
    }

    CLogLog(LOGWARNING, "Failed to acquire D-Bus name '%s'", dbus_name);
    goto fail;
  }

  return true;

fail:
  if(dbus_error_is_set(&error))
    dbus_error_free(&error);

  if (bus)
  {
    dbus_connection_close(bus);
    dbus_connection_unref(bus);
    bus = nullptr;
  }

  return false;
}

void OMXControl::dbus_disconnect()
{
  if (bus)
  {
    dbus_connection_close(bus);
    dbus_connection_unref(bus);
    bus = nullptr;
  }
}

enum ControlFlow OMXControl::getEvent()
{
  if (!bus)
    return CONTINUE;

  dispatch();
  DBusMessage *m = dbus_connection_pop_message(bus);
  if (m == nullptr)
    return CONTINUE;

  CLogLog(LOGDEBUG, "Popped message member: %s interface: %s type: %d path: %s", dbus_message_get_member(m), dbus_message_get_interface(m), dbus_message_get_type(m), dbus_message_get_path(m) );

  const char *method = dbus_message_get_member(m);
  if (method == nullptr)
    return CONTINUE;

  enum Action action = dbus_find_method(method);
  bool expects_response = !dbus_message_get_no_reply(m);

  DMessage message(m, expects_response);
  return handle_event(action, &message);
}


DMessage::DMessage(DBusMessage *message, bool res)
{
  m = message;
  needs_response = res;
}

DMessage::~DMessage()
{
  if(needs_response)
  {
    DBusMessage *reply = dbus_message_new_method_return(m);

    if(reply)
    {
      dbus_connection_send(bus, reply, nullptr);
      dbus_message_unref(reply);
    }
  }

  if(m_args)
    delete m_args;

  dbus_message_unref(m);
}

bool DMessage::get_arg(int type, void *value)
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

bool DMessage::get_arg_int(int *value)
{
  return get_arg(DBUS_TYPE_INT32, value);
}

bool DMessage::get_arg_int64(int64_t *value)
{
  return get_arg(DBUS_TYPE_INT64, value);
}

bool DMessage::get_arg_double(double *value)
{
  return get_arg(DBUS_TYPE_DOUBLE, value);
}

bool DMessage::get_arg_string(std::string &s)
{
  const char *value;
  bool r = get_arg(DBUS_TYPE_STRING, &value);
  if(r)
    s.assign(value);
  return r;
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
  DBusMessage *reply = dbus_message_new_error(m, name, msg);
  if(!reply)
    throw "memory error";

  dbus_connection_send(bus, reply, nullptr);
  dbus_message_unref(reply);
  needs_response = false;
}

void DMessage::respond_unknown_property()
{
  respond_error(DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
}


void DMessage::respond_unknown_method()
{
  respond_error(DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
}

void DMessage::respond_invalid_args()
{
  respond_error(DBUS_ERROR_INVALID_ARGS, "Invalid arguments");
}

void DMessage::respond_int64(int64_t value)
{
  respond(DBUS_TYPE_INT64, &value);
}

void DMessage::respond_double(double value)
{
  respond(DBUS_TYPE_DOUBLE, &value);
}

void DMessage::respond_bool(bool value)
{
  int t = value ? 1 : 0;
  respond(DBUS_TYPE_BOOLEAN, &t);
}

void DMessage::respond_string(const std::string &s)
{
  const char *value = s.c_str();
  respond(DBUS_TYPE_STRING, &value);
}

void DMessage::respond(int type, void *value)
{
  DBusMessage *reply = dbus_message_new_method_return(m);

  if (!reply)
    throw "Memory error";

  dbus_message_append_args(reply, type, value, DBUS_TYPE_INVALID);
  dbus_connection_send(bus, reply, nullptr);

  dbus_message_unref(reply);

  needs_response = false;
}

void DMessage::respond_array(const std::vector<std::string> &list)
{
  const char *char_list[list.size()];

  for(uint i = 0; i < list.size(); i++)
    char_list[i] = &list[i][0];

  respond_array(char_list, list.size());
}

void DMessage::respond_array(const char *array[], int size)
{
  DBusMessage *reply = dbus_message_new_method_return(m);

  if (!reply)
    throw "Memory error";

  dbus_message_append_args(reply, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, size, DBUS_TYPE_INVALID);
  dbus_connection_send(bus, reply, nullptr);
  dbus_message_unref(reply);

  needs_response = false;
}

void DMessage::send_metadata(const char *url, int64_t *duration)
{
  DBusMessage *reply = dbus_message_new_method_return(m);
  if(reply)
  {
    //Create iterator: Array of dict entries, composed of string (key)) and variant (value)
    DBusMessageIter array_cont, dict_cont, dict_entry_cont, var;
    dbus_message_iter_init_append(reply, &array_cont);
    dbus_message_iter_open_container(&array_cont, DBUS_TYPE_ARRAY, "{sv}", &dict_cont);

      //First dict entry: URI
      const char *key1 = "xesam:url";
      dbus_message_iter_open_container(&dict_cont, DBUS_TYPE_DICT_ENTRY, nullptr, &dict_entry_cont);
        dbus_message_iter_append_basic(&dict_entry_cont, DBUS_TYPE_STRING, &key1);
        dbus_message_iter_open_container(&dict_entry_cont, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &url);
        dbus_message_iter_close_container(&dict_entry_cont, &var);
      dbus_message_iter_close_container(&dict_cont, &dict_entry_cont);

      //Second dict entry: duration in us
      const char *key2 = "mpris:length";
      dbus_message_iter_open_container(&dict_cont, DBUS_TYPE_DICT_ENTRY, nullptr, &dict_entry_cont);
        dbus_message_iter_append_basic(&dict_entry_cont, DBUS_TYPE_STRING, &key2);
        dbus_message_iter_open_container(&dict_entry_cont, DBUS_TYPE_VARIANT, DBUS_TYPE_INT64_AS_STRING, &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_INT64, duration);
        dbus_message_iter_close_container(&dict_entry_cont, &var);
      dbus_message_iter_close_container(&dict_cont, &dict_entry_cont);
    dbus_message_iter_close_container(&array_cont, &dict_cont);

    //Send message
    needs_response = false;
    dbus_connection_send(bus, reply, nullptr);
    dbus_message_unref(reply);
  }
}
