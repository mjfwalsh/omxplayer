#define OMXPLAYER_DBUS_PATH_SERVER "/org/mpris/MediaPlayer2"  
#define OMXPLAYER_DBUS_INTERFACE_ROOT "org.mpris.MediaPlayer2"
#define OMXPLAYER_DBUS_INTERFACE_PLAYER "org.mpris.MediaPlayer2.Player"

#include <dbus/dbus.h>
#include <string>

class OMXPlayerAudio;
class OMXPlayerSubtitles;
class OMXReader;
class OMXClock;

#define MIN_RATE (1)
#define MAX_RATE (4 * DVD_PLAYSPEED_NORMAL)


class OMXControlResult {
  int key;
  int64_t arg = 0;
  const char *winarg = NULL;

public:
   OMXControlResult(int);
   OMXControlResult(int, int64_t);
   OMXControlResult(int, const char *);
   int getKey();
   int64_t getArg();
   const char *getWinArg();
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
  OMXControl();
  ~OMXControl();
  void init(OMXClock *av_clock, OMXPlayerSubtitles *player_subtitles, std::string (*filename)());
  bool connect(std::string& dbus_name);
  void set_reader(OMXReader *omx_reader);
  void set_audio(OMXPlayerAudio *player_audio);
  OMXControlResult getEvent();
private:
  enum DBusFunction {
    ROOT_QUIT,
    ROOT_RAISE,
    PROP_GET,
    PROP_SET,
    PROP_CAN_QUIT,
    PROP_FULLSCREEN,
    PROP_CAN_SET_FULLSCREEN,
    PROP_CAN_RAISE,
    PROP_HAS_TRACK_LIST,
    PROP_IDENTITY,
    PROP_SUPPORTED_URI_SCHEMES,
    PROP_SUPPORTED_MIME_TYPES,
    PROP_CAN_GO_NEXT,
    PROP_CAN_GO_PREVIOUS,
    PROP_CAN_SEEK,
    PROP_CAN_CONTROL,
    PROP_CAN_PLAY,
    PROP_CAN_PAUSE,
    PROP_PLAYBACK_STATUS,
    PROP_GET_SOURCE,
    PROP_VOLUME,
    PROP_MUTE,
    PROP_UNMUTE,
    PROP_POSITION,
    PROP_ASPECT,
    PROP_VIDEO_STREAM_COUNT,
    PROP_RES_WIDTH,
    PROP_RES_HEIGHT,
    PROP_DURATION,
    PROP_MINIMUM_RATE,
    PROP_MAXIMUM_RATE,
    PLAYER_GET_SOURCE,
    PLAYER_NEXT,
    PLAYER_PREVIOUS,
    PLAYER_PAUSE,
    PLAYER_PLAY,
    PLAYER_PLAY_PAUSE,
    PLAYER_STOP,
    PLAYER_SEEK,
    PLAYER_SET_POSITION,
    PLAYER_SET_ALPHA,
    PLAYER_SET_LAYER,
    PLAYER_SET_ASPECT_MODE,
    PLAYER_MUTE,
    PLAYER_UNMUTE,
    PLAYER_LIST_SUBTITLES,
    PLAYER_HIDE_VIDEO,
    PLAYER_UN_HIDE_VIDEO,
    PLAYER_LIST_AUDIO,
    PLAYER_LIST_VIDEO,
    PLAYER_SELECT_SUBTITLE,
    PLAYER_SELECT_AUDIO,
    PLAYER_SHOW_SUBTITLES,
    PLAYER_HIDE_SUBTITLES,
    PLAYER_OPEN_URI,
    PLAYER_ACTION,

    GET_ROOT_CAN_RAISE,
    GET_ROOT_CAN_QUIT,
    GET_ROOT_CAN_SET_FULLSCREEN,
    GET_ROOT_FULLSCREEN,
    GET_ROOT_HAS_TRACK_LIST,
    GET_ROOT_IDENTITY,
    GET_ROOT_SUPPORTED_URI_SCHEMES,
    GET_ROOT_SUPPORTED_MIME_TYPES,
    GET_PLAYER_CAN_GO_NEXT,
    GET_PLAYER_CAN_GO_PREVIOUS,
    GET_PLAYER_CAN_SEEK,
    GET_PLAYER_CAN_CONTROL,
    GET_PLAYER_CAN_PLAY,
    GET_PLAYER_CAN_PAUSE,
    GET_PLAYER_POSITION,
    GET_PLAYER_PLAYBACK_STATUS,
    GET_PLAYER_MINIMUM_RATE,
    GET_PLAYER_MAXIMUM_RATE,
    GET_PLAYER_RATE,
    GET_PLAYER_VOLUME,
    GET_PLAYER_METADATA,
    GET_PLAYER_ASPECT,
    GET_PLAYER_VIDEO_STREAM_COUNT,
    GET_PLAYER_RES_WIDTH,
    GET_PLAYER_RES_HEIGHT,
    GET_PLAYER_DURATION
  };

  std::unordered_map<std::string, std::string> interfaces;
  std::unordered_map<std::string, enum DBusFunction> table;

  void dispatch();
  int dbus_connect(std::string& dbus_name);
  void dbus_disconnect();
  OMXControlResult handle_event(DBusMessage *m, enum DBusFunction search_key);
  OMXControlResult SetProperty(DBusMessage *m);
  DBusHandlerResult dbus_respond_error(DBusMessage *m, const char *name, const char *msg);
  DBusHandlerResult dbus_respond_ok(DBusMessage *m);
  DBusHandlerResult dbus_respond_int64(DBusMessage *m, int64_t i);
  DBusHandlerResult dbus_respond_double(DBusMessage *m, double d);
  DBusHandlerResult dbus_respond_boolean(DBusMessage *m, int b);
  DBusHandlerResult dbus_respond_string(DBusMessage *m, const char *text);
  DBusHandlerResult dbus_respond_array(DBusMessage *m, const char *array[], int size);
};
