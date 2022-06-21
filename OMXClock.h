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

#ifndef _AVCLOCK_H_
#define _AVCLOCK_H_

#include "OMXCore.h"

#define DVD_SEC_TO_MICROSEC(x) ((x)       * 1000000)
#define DVD_MILLISEC_TO_SEC(x) ((double)(x)       * 1000)

#define DVD_PLAYSPEED_PAUSE       0       // frame stepping
#define DVD_PLAYSPEED_NORMAL      1000

static inline OMX_TICKS ToOMXTime(int64_t pts)
{
  OMX_TICKS ticks;
  ticks.nLowPart = pts;
  ticks.nHighPart = ((uint64_t)(pts) >> 32);
  return ticks;
}
static inline int64_t FromOMXTime(OMX_TICKS ticks)
{
  int64_t pts = ticks.nLowPart | ((uint64_t)(ticks.nHighPart) << 32);
  return pts;
}

class OMXClock
{
protected:
  bool              m_pause;
  pthread_mutex_t   m_lock;
  int               m_omx_speed;
  OMX_U32           m_WaitMask;
  OMX_TIME_CLOCKSTATE   m_eState;
  OMX_TIME_REFCLOCKTYPE m_eClock;
private:
  COMXCoreComponent m_omx_clock;
  int64_t            m_last_media_time;
  int64_t            m_last_media_time_read;

public:
  OMXClock();
  ~OMXClock();

  bool OMXIsPaused() { return m_pause; };
  bool OMXStop(bool lock = true);
  bool OMXStep(int steps = 1, bool lock = true);
  bool OMXReset(bool has_video, bool has_audio, bool lock = true);
  int64_t OMXMediaTime(bool lock = true);
  bool OMXMediaTime(int64_t pts, bool lock = true);
  bool OMXPause(bool lock = true);
  bool OMXResume(bool lock = true);
  bool OMXSetSpeed(int speed, bool lock = true, bool pause_resume = false);
  int  OMXPlaySpeed() { return m_omx_speed; };
  COMXCoreComponent *GetOMXClock();
  bool OMXStateExecute(bool lock = true);
  void OMXStateIdle(bool lock = true);
  bool HDMIClockSync(bool lock = true);
  static int64_t CurrentHostCounter();
  static int64_t GetAbsoluteClock();
  static void OMXSleep(unsigned int dwMilliSeconds);
private:
  void Lock();
  void UnLock();
  void OMXSetClockPorts(OMX_TIME_CONFIG_CLOCKSTATETYPE *clock, bool has_video, bool has_audio);
  bool OMXSetReferenceClock(bool has_audio, bool lock = true);
};

#endif
