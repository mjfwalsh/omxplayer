/*
 *      Copyright (C) 2005-2013 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "OMXClock.h"
#include "utils/log.h"
#include "utils/SingleLock.h"

#include <errno.h>

#define OMX_PRE_ROLL 200

OMXClock::OMXClock()
:
m_pause(false),
m_omx_speed(DVD_PLAYSPEED_NORMAL),
m_WaitMask(0),
m_eState(OMX_TIME_ClockStateStopped),
m_eClock(OMX_TIME_RefClockNone),
m_last_media_time(0),
m_last_media_time_read(0)
{
  if(!m_omx_clock.Initialize("OMX.broadcom.clock", OMX_IndexParamOtherInit))
    throw "Failed to initialise media clock";
}

OMXClock::~OMXClock()
{
  Stop();
  StateIdle();

  if(m_omx_clock.GetComponent() != NULL)
    m_omx_clock.Deinitialize();
}

void OMXClock::SetClockPorts(OMX_TIME_CONFIG_CLOCKSTATETYPE *clock, bool has_video, bool has_audio)
{
  if(m_omx_clock.GetComponent() == NULL)
    return;

  if(!clock)
    return;

  clock->nWaitMask = 0;

  if(has_audio)
  {
    clock->nWaitMask |= OMX_CLOCKPORT0;
  }

  if(has_video)
  {
    clock->nWaitMask |= OMX_CLOCKPORT1;
  }
}

bool OMXClock::SetReferenceClock(bool has_audio)
{
  CSingleLock lock(m_lock);

  bool ret = true;
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE refClock;
  OMX_INIT_STRUCTURE(refClock);

  if(has_audio)
    refClock.eClock = OMX_TIME_RefClockAudio;
  else
    refClock.eClock = OMX_TIME_RefClockVideo;

  if (refClock.eClock != m_eClock)
  {
    CLogLog(LOGNOTICE, "OMXClock using %s as reference", refClock.eClock == OMX_TIME_RefClockVideo ? "video" : "audio");

    omx_err = m_omx_clock.SetConfig(OMX_IndexConfigTimeActiveRefClock, &refClock);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "OMXClock::SetReferenceClock error setting OMX_IndexConfigTimeActiveRefClock");
      ret = false;
    }
    m_eClock = refClock.eClock;
  }
  m_last_media_time = 0;

  return ret;
}

bool OMXClock::StateExecute()
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  CSingleLock lock(m_lock);

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  if(m_omx_clock.GetState() != OMX_StateExecuting)
  {
    StateIdle();

    omx_err = m_omx_clock.SetStateForComponent(OMX_StateExecuting);
    if (omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "OMXClock::StateExecute m_omx_clock.SetStateForComponent");
      return false;
    }
  }

  m_last_media_time = 0;

  return true;
}

void OMXClock::StateIdle()
{
  if(m_omx_clock.GetComponent() == NULL)
    return;

  CSingleLock lock(m_lock);

  if(m_omx_clock.GetState() != OMX_StateIdle)
    m_omx_clock.SetStateForComponent(OMX_StateIdle);

  m_last_media_time = 0;
}

COMXCoreComponent *OMXClock::GetOMXClock()
{
  return &m_omx_clock;
}

bool  OMXClock::Stop()
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  CSingleLock lock(m_lock);

  CLogLog(LOGDEBUG, "OMXClock::Stop");

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_TIME_CONFIG_CLOCKSTATETYPE clock;
  OMX_INIT_STRUCTURE(clock);

  clock.eState      = OMX_TIME_ClockStateStopped;
  clock.nOffset     = ToOMXTime(-1000LL * OMX_PRE_ROLL);

  omx_err = m_omx_clock.SetConfig(OMX_IndexConfigTimeClockState, &clock);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "OMXClock::Stop error setting OMX_IndexConfigTimeClockState");
    return false;
  }
  m_eState = clock.eState;

  m_last_media_time = 0;

  return true;
}

bool OMXClock::Step(int steps /* = 1 */)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  CSingleLock lock(m_lock);

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_PARAM_U32TYPE param;
  OMX_INIT_STRUCTURE(param);

  param.nPortIndex = OMX_ALL;
  param.nU32 = steps;

  omx_err = m_omx_clock.SetConfig(OMX_IndexConfigSingleStep, &param);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "OMXClock::Error setting OMX_IndexConfigSingleStep");
    return false;
  }

  m_last_media_time = 0;

  CLogLog(LOGDEBUG, "OMXClock::Step (%d)", steps);
  return true;
}

bool OMXClock::Reset(bool has_video, bool has_audio)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  CSingleLock lock(m_lock);

  if(!SetReferenceClock(has_audio))
  {
    return false;
  }

  if (m_eState == OMX_TIME_ClockStateStopped)
  {
    OMX_TIME_CONFIG_CLOCKSTATETYPE clock;
    OMX_INIT_STRUCTURE(clock);

    clock.eState    = OMX_TIME_ClockStateWaitingForStartTime;
    clock.nOffset   = ToOMXTime(-1000LL * OMX_PRE_ROLL);

    SetClockPorts(&clock, has_video, has_audio);

    if(clock.nWaitMask)
    {
      OMX_ERRORTYPE omx_err = m_omx_clock.SetConfig(OMX_IndexConfigTimeClockState, &clock);
      if(omx_err != OMX_ErrorNone)
      {
        CLogLog(LOGERROR, "OMXClock::Reset error setting OMX_IndexConfigTimeClockState");
        return false;
      }
      CLogLog(LOGDEBUG, "OMXClock::Reset audio / video : %d / %d wait mask %d->%d state : %d->%d",
          has_audio, has_video, m_WaitMask, clock.nWaitMask, m_eState, clock.eState);
      if (m_eState != OMX_TIME_ClockStateStopped)
        m_WaitMask = clock.nWaitMask;
      m_eState = clock.eState;
    }
  }

  m_last_media_time = 0;

  return true;
}

int64_t OMXClock::GetMediaTime()
{
  int64_t pts = 0;
  if(m_omx_clock.GetComponent() == NULL)
    return 0;

  int64_t now = GetAbsoluteClock();
  if (now - m_last_media_time_read > 100000 || m_last_media_time == 0)
  {
    CSingleLock lock(m_lock);

    OMX_ERRORTYPE omx_err = OMX_ErrorNone;

    OMX_TIME_CONFIG_TIMESTAMPTYPE timeStamp;
    OMX_INIT_STRUCTURE(timeStamp);
    timeStamp.nPortIndex = m_omx_clock.GetInputPort();

    omx_err = m_omx_clock.GetConfig(OMX_IndexConfigTimeCurrentMediaTime, &timeStamp);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "OMXClock::MediaTime error getting OMX_IndexConfigTimeCurrentMediaTime");
      return 0;
    }

    pts = FromOMXTime(timeStamp.nTimestamp);
    //CLogLog(LOGINFO, "OMXClock::MediaTime %.2f (%.2f, %.2f)", pts, m_last_media_time, now - m_last_media_time_read);
    m_last_media_time = pts;
    m_last_media_time_read = now;
  }
  else
  {
    double speed = m_pause ? 0.0 : m_omx_speed;
    pts = m_last_media_time + (now - m_last_media_time_read) * speed;
    //CLogLog(LOGINFO, "OMXClock::MediaTime cached %.2f (%.2f, %.2f)", pts, m_last_media_time, now - m_last_media_time_read);
  }
  return pts;
}

// Set the media time, so calls to get media time use the updated value,
// useful after a seek so mediatime is updated immediately (rather than waiting for first decoded packet)
bool OMXClock::SetMediaTime(int64_t pts)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  CSingleLock lock(m_lock);

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_INDEXTYPE index;
  OMX_TIME_CONFIG_TIMESTAMPTYPE timeStamp;
  OMX_INIT_STRUCTURE(timeStamp);
  timeStamp.nPortIndex = m_omx_clock.GetInputPort();

  if(m_eClock == OMX_TIME_RefClockAudio)
    index = OMX_IndexConfigTimeCurrentAudioReference;
  else
    index = OMX_IndexConfigTimeCurrentVideoReference;

  timeStamp.nTimestamp = ToOMXTime(pts);

  omx_err = m_omx_clock.SetConfig(index, &timeStamp);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "OMXClock::SetMediaTime error setting %s", index == OMX_IndexConfigTimeCurrentAudioReference ?
       "OMX_IndexConfigTimeCurrentAudioReference":"OMX_IndexConfigTimeCurrentVideoReference");
    return false;
  }

  CLogLog(LOGDEBUG, "OMXClock::SetMediaTime set config %s = %lld", index == OMX_IndexConfigTimeCurrentAudioReference ?
       "OMX_IndexConfigTimeCurrentAudioReference":"OMX_IndexConfigTimeCurrentVideoReference", pts);

  m_last_media_time = 0;

  return true;
}

bool OMXClock::Pause()
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(!m_pause)
  {
    CSingleLock lock(m_lock);

    if(SetSpeed(0.0f))
      m_pause = true;

    m_last_media_time = 0;
  }
  return m_pause == true;
}

bool OMXClock::Resume()
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  if(m_pause)
  {
    CSingleLock lock(m_lock);

    if(SetSpeed(m_omx_speed))
      m_pause = false;

    m_last_media_time = 0;
  }
  return m_pause == false;
}

bool OMXClock::SetSpeed(float speed)
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  CSingleLock lock(m_lock);

  CLogLog(LOGDEBUG, "OMXClock::SetSpeed(%.2f)", speed);

  if(speed > 0.0f)
    m_omx_speed = speed;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_TIME_CONFIG_SCALETYPE scaleType;
  OMX_INIT_STRUCTURE(scaleType);

  scaleType.xScale = speed * 65536.0;
  omx_err = m_omx_clock.SetConfig(OMX_IndexConfigTimeScale, &scaleType);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGDEBUG, "OMXClock::SetSpeed error setting OMX_IndexConfigTimeClockState");
    return false;
  }

  m_last_media_time = 0;

  return true;
}

bool OMXClock::HDMIClockSync()
{
  if(m_omx_clock.GetComponent() == NULL)
    return false;

  CSingleLock lock(m_lock);

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_CONFIG_LATENCYTARGETTYPE latencyTarget;
  OMX_INIT_STRUCTURE(latencyTarget);

  latencyTarget.nPortIndex = OMX_ALL;
  latencyTarget.bEnabled = OMX_TRUE;
  latencyTarget.nFilter = 10;
  latencyTarget.nTarget = 0;
  latencyTarget.nShift = 3;
  latencyTarget.nSpeedFactor = -60;
  latencyTarget.nInterFactor = 100;
  latencyTarget.nAdjCap = 100;

  omx_err = m_omx_clock.SetConfig(OMX_IndexConfigLatencyTarget, &latencyTarget);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "OMXClock::Speed error setting OMX_IndexConfigLatencyTarget");
    return false;
  }

  m_last_media_time = 0;

  return true;
}

void OMXClock::Sleep(unsigned int dwMilliSeconds)
{
  struct timespec req;
  req.tv_sec = dwMilliSeconds / 1000;
  req.tv_nsec = (dwMilliSeconds % 1000) * 1000000;

  while ( nanosleep(&req, &req) == -1 && errno == EINTR && (req.tv_nsec > 0 || req.tv_sec > 0));
}

int64_t OMXClock::CurrentHostCounter()
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return( ((int64_t)now.tv_sec * 1000000000LL) + now.tv_nsec );
}

int64_t OMXClock::GetAbsoluteClock()
{
  return CurrentHostCounter()/1000;
}
