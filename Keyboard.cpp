#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "utils/log.h"
#include "Keyboard.h"
#include "KeyConfig.h"

Keyboard::Keyboard(const char *filename)
{
  // setKeymap
  KeyConfig::buildKeymap(filename, m_keymap);

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

    if(chnum > 0)
    {
      int result = 0;

      if(chnum == 1)
      {
        result = ch[0];
      }
      else if(ch[0] == '\e')
      {
        if(chnum > 5) chnum = 5;
        for(int i = 1; i < chnum; i++)
          result = (result << 8) | ch[i];
      }
      else
      {
        if(chnum > 4) chnum = 4;
        for(int i = 0; i < chnum; i++)
          result = (result << 8) | ch[i];
      }

      CLogLog(LOGDEBUG, "Keyboard: character %c (0x%x)", result, result);

      int action = m_keymap[result];
      if(action != 0)
        m_action = action;
    }

    Sleep(20);
  }
}

enum Action Keyboard::getEvent()
{
  return (enum Action)m_action.exchange(-1);
}
