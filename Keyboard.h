#include "OMXThread.h"
#include "KeyConfig.h"
#include <unordered_map>
#include <atomic>
#include <termios.h>

class Keyboard : public OMXThread
{
 protected:
  struct termios orig_termios;
  int orig_fl;
  std::atomic<int> m_action{-1};

  std::unordered_map<int,int> m_keymap;
 public:
  explicit Keyboard(const char *filename);
  ~Keyboard();
  void Process() override;
  void Sleep(unsigned int dwMilliSeconds);
  enum Action getEvent();
};
