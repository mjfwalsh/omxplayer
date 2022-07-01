#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <dbus/dbus.h>
#include <errno.h>

#include "utils/log.h"
#include "Keyboard.h"
#include "KeyConfig.h"

Keyboard::Keyboard(std::string &filename)
{
  // setKeymap
  if(filename.empty()) {
    KeyConfig::buildDefaultKeymap(m_keymap);
  } else {
    KeyConfig::parseConfigFile(filename, m_keymap);
  }

  if (isatty(STDIN_FILENO)) 
  {
    struct termios new_termios;

    tcgetattr(STDIN_FILENO, &orig_termios);

    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO | ECHOCTL | ECHONL);
    new_termios.c_cflag |= HUPCL;
    new_termios.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    orig_fl = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, FNDELAY);
  } 
  else 
  {    
    orig_fl = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, orig_fl | O_NONBLOCK);
  }

  Create();
}

Keyboard::~Keyboard()
{
  m_bAbort = true;
  if (ThreadHandle())
  {
    StopThread();
  }
  restore_term();
}

void Keyboard::restore_term() 
{
  if (isatty(STDIN_FILENO)) 
  {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    fcntl(STDIN_FILENO, F_SETFL, orig_fl);
  } 
  else 
  {
    fcntl(STDIN_FILENO, F_SETFL, orig_fl);
  }
}

void Keyboard::Sleep(unsigned int dwMilliSeconds)
{
  struct timespec req;
  req.tv_sec = dwMilliSeconds / 1000;
  req.tv_nsec = (dwMilliSeconds % 1000) * 1000000;

  while ( nanosleep(&req, &req) == -1 && errno == EINTR && (req.tv_nsec > 0 || req.tv_sec > 0));
}

void Keyboard::Process() 
{
  while(!m_bAbort)
  {
    int ch[8];
    int chnum = 0;

    while ((ch[chnum] = getchar()) != EOF) chnum++;

    if (chnum > 1) ch[0] = ch[chnum - 1] | (ch[chnum - 2] << 8);

    if (chnum > 0)
      CLogLog(LOGDEBUG, "Keyboard: character %c (0x%x)", ch[0], ch[0]);

    if (m_keymap[ch[0]] != 0)
          m_action = m_keymap[ch[0]];
    else
      Sleep(20);
  }
}

int Keyboard::getEvent()
{
  return m_action.exchange(-1);
}
