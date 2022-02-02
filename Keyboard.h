#define OMXPLAYER_DBUS_PATH_SERVER "/org/mpris/MediaPlayer2"  
#define OMXPLAYER_DBUS_INTERFACE_ROOT "org.mpris.MediaPlayer2"
#define OMXPLAYER_DBUS_INTERFACE_PLAYER "org.mpris.MediaPlayer2.Player"

#include "OMXThread.h"
#include <unordered_map>
#include <string>
#include <termios.h>

class Keyboard : public OMXThread
{
 protected:
  struct termios orig_termios;
  int orig_fl;
  int m_action = -1;
  DBusConnection *conn;
  std::unordered_map<int,int> m_keymap;
  std::string m_dbus_name;
 public:
  ~Keyboard();
  void Init(std::string &filename, const std::string &dbus_name);
  void Process();
  void Sleep(unsigned int dwMilliSeconds);
  int getEvent();
 private:
  void restore_term();
  void send_action(int action);
  int dbus_connect();
  void dbus_disconnect();
  bool m_init = false;
};
