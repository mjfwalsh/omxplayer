#pragma once

#include "KeyConfig.h"
#include "omxplayer.h"

#include <stdint.h>
#include <vector>
#include <string>

struct DBusMessage;
struct DBusMessageIter;

class DMessage
{
private:
  DBusMessage *m;
  DBusMessageIter *m_args = NULL;

public:
  explicit DMessage(DBusMessage *message);
  ~DMessage();

  operator DBusMessage*() const { return m; }

  bool get_arg_int(int *value);
  bool get_arg_int64(int64_t *value);
  bool get_arg_double(double *value);
  bool get_arg_string(const char **value);

  bool ignore_arg();

  void respond_unknown_property();
  void respond_unknown_method();
  void respond_invalid_args();

  void respond_int64(int64_t value);
  void respond_double(double value);
  void respond_bool(bool value);
  void respond_string(const char *value);

  void respond_array(std::vector<std::string> &list);
  void respond_array(const char *array[], int size);
  bool needs_response = true;

  void send_metadata(const char *url, int64_t *duration);
private:
  void respond(int type, void *value);
  bool get_arg(int type, void *value);
  void respond_error(const char *name, const char *msg);
};


class OMXControl
{
public:
  ~OMXControl();
  bool connect(const char *dbus_name);
  enum ControlFlow getEvent();
  operator bool() const;
private:
  void dispatch();
  bool dbus_connect(const char *dbus_name);
  void dbus_disconnect();
};
