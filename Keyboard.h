#include "OMXThread.h"
#include <unordered_map>
#include <string>
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
  explicit Keyboard(std::string &filename);
  ~Keyboard();
  void Process() override;
  void Sleep(unsigned int dwMilliSeconds);
  int getEvent();
 private:
  void restore_term();
};
