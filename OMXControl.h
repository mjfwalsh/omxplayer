#define OMXPLAYER_DBUS_INTERFACE_PLAYER "org.mpris.MediaPlayer2.Player"

#include <dbus/dbus.h>
#include <string>

#include "DbusCommandSearch.h"

class OMXPlayerAudio;
class OMXPlayerSubtitles;
class OMXReader;
class OMXClock;

#define MIN_RATE (1)
#define MAX_RATE (4 * DVD_PLAYSPEED_NORMAL)


class OMXControlResult {
  int key;

  union {
    int64_t int64arg;
    double doublearg;
    const char *strarg;
  } v;

public:
   OMXControlResult(int);
   OMXControlResult(int, int64_t);
   OMXControlResult(int, double);
   OMXControlResult(int, const char *);
   int getKey();
   int64_t getArg();
   double getDoubleArg();
   const char *getStrArg();
};

class OMXControl
{
protected:
  DBusConnection     *bus;
  OMXClock           *clock;
  OMXPlayerAudio     *audio;
  OMXReader          *reader;
  OMXPlayerSubtitles *subtitles;
  std::string (*get_filename)();
public:
  ~OMXControl();
  void init(OMXClock *av_clock, OMXPlayerSubtitles *player_subtitles, std::string (*filename)());
  bool connect(std::string& dbus_name);
  void set_reader(OMXReader *omx_reader);
  void set_audio(OMXPlayerAudio *player_audio);
  OMXControlResult getEvent();
private:
  void dispatch();
  int dbus_connect(std::string& dbus_name);
  void dbus_disconnect();
  OMXControlResult handle_event(DBusMessage *m, enum DBusMethod search_key);
  OMXControlResult SetProperty(DBusMessage *m);
  DBusHandlerResult dbus_respond_error(DBusMessage *m, const char *name, const char *msg);
  DBusHandlerResult dbus_respond_ok(DBusMessage *m);
  DBusHandlerResult dbus_respond_int64(DBusMessage *m, int64_t i);
  DBusHandlerResult dbus_respond_double(DBusMessage *m, double d);
  DBusHandlerResult dbus_respond_boolean(DBusMessage *m, int b);
  DBusHandlerResult dbus_respond_string(DBusMessage *m, const char *text);
  DBusHandlerResult dbus_respond_array(DBusMessage *m, const char *array[], int size);
};
