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
    int intarg;
    int64_t int64arg;
    double doublearg;
    const char *strarg;
  } v;

public:
   OMXControlResult(int);
   OMXControlResult(int, int);
   OMXControlResult(int, int64_t);
   OMXControlResult(int, double);
   OMXControlResult(int, const char *);
   int getKey();
   int getIntArg();
   int64_t getInt64Arg();
   double getDoubleArg();
   const char *getStrArg();
};

class DMessage;

class OMXControl
{
protected:
  DBusConnection     *bus;
  OMXClock           *clock;
  OMXPlayerAudio     *audio;
  OMXReader          *reader;
  OMXPlayerSubtitles *subtitles;
  std::string& (*get_filename)();
  int (*get_approx_speed)(double &s);
public:
  ~OMXControl();
  void init(OMXClock *av_clock, OMXPlayerSubtitles *player_subtitles, std::string& (*filename)(), int (*speed)(double &s));
  bool connect(std::string& dbus_name);
  void set_reader(OMXReader *omx_reader);
  void set_audio(OMXPlayerAudio *player_audio);
  OMXControlResult getEvent();
private:
  void dispatch();
  int dbus_connect(std::string& dbus_name);
  void dbus_disconnect();
  OMXControlResult handle_event(DMessage &m, enum DBusMethod search_key);
  OMXControlResult SetProperty(DMessage &m);
};
