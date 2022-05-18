/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <cstdlib>
#include <time.h>
#include <sys/time.h>
#include <cstring>
#include <stdarg.h>
#include <pthread.h>
#include <cstdint>

#include "log.h"

static FILE*       m_stream         = NULL;
static bool        m_file_is_open   = false;
static int         m_logLevel       = LOGNONE;

static pthread_mutex_t   m_log_mutex;

static char levelNames[][8] =
{"NONE", "FATAL", "SEVERE", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG"};

bool logging_enabled = false;

static void CLogClose()
{
  if (m_file_is_open)
  {
    fclose(m_stream);
    m_file_is_open = false;
  }
  m_stream = NULL;

  pthread_mutex_destroy(&m_log_mutex);
}

void _CLogLog(int loglevel, const char *format, ... )
{
  if (m_stream == NULL || loglevel > m_logLevel)
    return;

  pthread_mutex_lock(&m_log_mutex);

  struct timeval now;
  gettimeofday(&now, NULL);
  struct tm *time = localtime( &now.tv_sec );
  uint64_t stamp = now.tv_usec + now.tv_sec * 1000000;

  fprintf(m_stream, "%02d:%02d:%02d T:%llu %7s: ", time->tm_hour, time->tm_min, time->tm_sec, stamp, levelNames[loglevel]);

  va_list va;
  va_start(va, format);
  vfprintf(m_stream, format, va);
  va_end(va);

  fputs("\n", m_stream);
  fflush(m_stream);

  pthread_mutex_unlock(&m_log_mutex);
}

bool CLogInit(int level, const char* path)
{
  if(level == LOGNONE) return false;

  pthread_mutex_init(&m_log_mutex, NULL);

  m_logLevel = level;
  logging_enabled = true;
  atexit(CLogClose);

  if(path == NULL)
  {
    m_stream = stdout;
    m_file_is_open = logging_enabled = true;
  }
  else
  {
    m_stream = fopen(path, "w");
    m_file_is_open = logging_enabled = m_stream != NULL;
  }

  return logging_enabled;
}
