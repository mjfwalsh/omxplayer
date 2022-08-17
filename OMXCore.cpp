/*
 *      Copyright (C) 2010-2013 Team XBMCn
 *      http://xbmc.org
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

#include <sys/time.h>
#include <assert.h>

#include "OMXCore.h"
#include "utils/log.h"

#include "linux/OMXAlsa.h"

//#define OMX_DEBUG_EVENTS
//#define OMX_DEBUG_EVENTHANDLER

////////////////////////////////////////////////////////////////////////////////////////////
#define CLASSNAME "COMXCoreComponent"
////////////////////////////////////////////////////////////////////////////////////////////

static void add_timespecs(struct timespec &time, long millisecs)
{
   long long nsec = time.tv_nsec + (long long)millisecs * 1000000;
   while (nsec > 1000000000)
   {
      time.tv_sec += 1;
      nsec -= 1000000000;
   }
   time.tv_nsec = nsec;
}


void COMXCoreTunel::Initialize(COMXCoreComponent *src_component, unsigned int src_port, COMXCoreComponent *dst_component, unsigned int dst_port)
{
  m_src_component  = src_component;
  m_src_port    = src_port;
  m_dst_component  = dst_component;
  m_dst_port    = dst_port;
}

OMX_ERRORTYPE COMXCoreTunel::Deestablish(bool noWait)
{
  if(!m_src_component || !m_dst_component || !IsInitialized())
    return OMX_ErrorUndefined;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  if(m_src_component->GetComponent())
  {
    omx_err = m_src_component->DisablePort(m_src_port, false);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Deestablish - Error disable port %d on component %s omx_err(0x%08x)",
          m_src_port, m_src_component->GetName(), (int)omx_err);
    }
  }

  if(m_dst_component->GetComponent())
  {
    omx_err = m_dst_component->DisablePort(m_dst_port, false);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Deestablish - Error disable port %d on component %s omx_err(0x%08x)",
          m_dst_port, m_dst_component->GetName(), (int)omx_err);
    }
  }

  if(m_src_component->GetComponent())
  {
    omx_err = m_src_component->WaitForCommand(OMX_CommandPortDisable, m_src_port);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Deestablish - Error WaitForCommand port %d on component %s omx_err(0x%08x)",
          m_dst_port, m_src_component->GetName(), (int)omx_err);
      return omx_err;
    }
  }

  if(m_dst_component->GetComponent())
  {
    omx_err = m_dst_component->WaitForCommand(OMX_CommandPortDisable, m_dst_port);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Deestablish - Error WaitForCommand port %d on component %s omx_err(0x%08x)",
          m_dst_port, m_dst_component->GetName(), (int)omx_err);
      return omx_err;
    }
  }

  if(m_src_component->GetComponent())
  {
    omx_err = OMX_SetupTunnel(m_src_component->GetComponent(), m_src_port, NULL, 0);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Deestablish - could not unset tunnel on comp src %s port %d omx_err(0x%08x)",
          m_src_component->GetName(), m_src_port, (int)omx_err);
    }
  }

  if(m_dst_component->GetComponent())
  {
    omx_err = OMX_SetupTunnel(m_dst_component->GetComponent(), m_dst_port, NULL, 0);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Deestablish - could not unset tunnel on comp dst %s port %d omx_err(0x%08x)",
          m_dst_component->GetName(), m_dst_port, (int)omx_err);
    }
  }

  m_tunnel_set = false;

  return OMX_ErrorNone;
}

OMX_ERRORTYPE COMXCoreTunel::Establish()
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_PARAM_U32TYPE param;
  OMX_INIT_STRUCTURE(param);

  if(!m_src_component || !m_dst_component)
  {
    return OMX_ErrorUndefined;
  }

  if(m_src_component->GetState() == OMX_StateLoaded)
  {
    omx_err = m_src_component->SetStateForComponent(OMX_StateIdle);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Establish - Error setting state to idle %s omx_err(0x%08x)",
          m_src_component->GetName(), (int)omx_err);
      return omx_err;
    }
  }

  if(m_src_component->GetComponent() && m_dst_component->GetComponent())
  {
    omx_err = OMX_SetupTunnel(m_src_component->GetComponent(), m_src_port, m_dst_component->GetComponent(), m_dst_port);
    if(omx_err != OMX_ErrorNone) 
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Establish - could not setup tunnel src %s port %d dst %s port %d omx_err(0x%08x)",
          m_src_component->GetName(), m_src_port, m_dst_component->GetName(), m_dst_port, (int)omx_err);
      return omx_err;
    }
  }
  else
  {
    CLogLog(LOGERROR, "COMXCoreTunel::Establish - could not setup tunnel");
    return OMX_ErrorUndefined;
  }

  m_tunnel_set = true;

  if(m_src_component->GetComponent())
  {
    omx_err = m_src_component->EnablePort(m_src_port, false);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Establish - Error enable port %d on component %s omx_err(0x%08x)",
          m_src_port, m_src_component->GetName(), (int)omx_err);
      return omx_err;
    }
  }

  if(m_dst_component->GetComponent())
  {
    omx_err = m_dst_component->EnablePort(m_dst_port, false);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreTunel::Establish - Error enable port %d on component %s omx_err(0x%08x)",
          m_dst_port, m_dst_component->GetName(), (int)omx_err);
      return omx_err;
    }
  }

  if(m_dst_component->GetComponent())
  {
    omx_err = m_dst_component->WaitForCommand(OMX_CommandPortEnable, m_dst_port);
    if(omx_err != OMX_ErrorNone)
    {
      return omx_err;
    }

    if(m_dst_component->GetState() == OMX_StateLoaded)
    {
      omx_err = m_dst_component->SetStateForComponent(OMX_StateIdle);
      if(omx_err != OMX_ErrorNone)
      {
        CLogLog(LOGERROR, "COMXCoreComponent::Establish - Error setting state to idle %s omx_err(0x%08x)",
            m_src_component->GetName(), (int)omx_err);
        return omx_err;
      }
    }
  }

  if(m_src_component->GetComponent())
  {
    omx_err = m_src_component->WaitForCommand(OMX_CommandPortEnable, m_src_port);
    if(omx_err != OMX_ErrorNone)
    {
      return omx_err;
    }
  }

  return OMX_ErrorNone;
}

////////////////////////////////////////////////////////////////////////////////////////////

COMXCoreComponent::COMXCoreComponent()
{
  pthread_mutex_init(&m_omx_input_mutex, NULL);
  pthread_mutex_init(&m_omx_output_mutex, NULL);
  pthread_mutex_init(&m_omx_event_mutex, NULL);
  pthread_mutex_init(&m_omx_eos_mutex, NULL);
  pthread_cond_init(&m_input_buffer_cond, NULL);
  pthread_cond_init(&m_output_buffer_cond, NULL);
  pthread_cond_init(&m_omx_event_cond, NULL);
}

COMXCoreComponent::~COMXCoreComponent()
{
  Deinitialize();

  pthread_mutex_destroy(&m_omx_input_mutex);
  pthread_mutex_destroy(&m_omx_output_mutex);
  pthread_mutex_destroy(&m_omx_event_mutex);
  pthread_mutex_destroy(&m_omx_eos_mutex);
  pthread_cond_destroy(&m_input_buffer_cond);
  pthread_cond_destroy(&m_output_buffer_cond);
  pthread_cond_destroy(&m_omx_event_cond);
}

void COMXCoreComponent::TransitionToStateLoaded()
{
  if(!m_handle)
    return;

  if(GetState() != OMX_StateLoaded && GetState() != OMX_StateIdle)
    SetStateForComponent(OMX_StateIdle);

  if(GetState() != OMX_StateLoaded)
    SetStateForComponent(OMX_StateLoaded);
}

OMX_ERRORTYPE COMXCoreComponent::EmptyThisBuffer(OMX_BUFFERHEADERTYPE *omx_buffer)
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  #if defined(OMX_DEBUG_EVENTHANDLER)
  CLogLog(LOGDEBUG, "COMXCoreComponent::EmptyThisBuffer component(%s) %p", m_componentName, omx_buffer);
  #endif
  if(!m_handle || !omx_buffer)
    return OMX_ErrorUndefined;

  omx_err = OMX_EmptyThisBuffer(m_handle, omx_buffer);
  if (omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::EmptyThisBuffer component(%s) - failed with result(0x%x)",
        m_componentName, omx_err);
  }

  return omx_err;
}

void COMXCoreComponent::FlushAll()
{
  FlushInput();
  FlushOutput();
}

void COMXCoreComponent::FlushInput()
{
  if(!m_handle || m_resource_error)
    return;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  omx_err = OMX_SendCommand(m_handle, OMX_CommandFlush, m_input_port, NULL);

  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::FlushInput - Error on component %s omx_err(0x%08x)",
              m_componentName, (int)omx_err);
  }
  omx_err = WaitForCommand(OMX_CommandFlush, m_input_port);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::FlushInput - %s WaitForCommand omx_err(0x%08x)",
              m_componentName, (int)omx_err);
  }
}

void COMXCoreComponent::FlushOutput()
{
  if(!m_handle || m_resource_error)
    return;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  omx_err = OMX_SendCommand(m_handle, OMX_CommandFlush, m_output_port, NULL);

  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::FlushOutput - Error on component %s omx_err(0x%08x)",
              m_componentName, (int)omx_err);
  }
  omx_err = WaitForCommand(OMX_CommandFlush, m_output_port);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::FlushOutput - %s WaitForCommand omx_err(0x%08x)",
              m_componentName, (int)omx_err);
  }
}

// timeout in milliseconds
OMX_BUFFERHEADERTYPE *COMXCoreComponent::GetInputBuffer(long timeout /*=200*/)
{
  OMX_BUFFERHEADERTYPE *omx_input_buffer = NULL;

  if(!m_handle)
    return NULL;

  pthread_mutex_lock(&m_omx_input_mutex);
  struct timespec endtime;
  clock_gettime(CLOCK_REALTIME, &endtime);
  add_timespecs(endtime, timeout);
  while (!m_flush_input)
  {
    if (m_resource_error)
      break;
    if(!m_omx_input_avaliable.empty())
    {
      omx_input_buffer = m_omx_input_avaliable.front();
      m_omx_input_avaliable.pop();
      break;
    }

    int retcode = pthread_cond_timedwait(&m_input_buffer_cond, &m_omx_input_mutex, &endtime);
    if (retcode != 0) {
      if (timeout != 0)
        CLogLog(LOGERROR, "COMXCoreComponent::GetInputBuffer %s wait event timeout", m_componentName);
      break;
    }
  }
  pthread_mutex_unlock(&m_omx_input_mutex);
  return omx_input_buffer;
}


OMX_ERRORTYPE COMXCoreComponent::WaitForInputDone(long timeout /*=200*/)
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  pthread_mutex_lock(&m_omx_input_mutex);
  struct timespec endtime;
  clock_gettime(CLOCK_REALTIME, &endtime);
  add_timespecs(endtime, timeout);
  while (m_input_buffer_count != m_omx_input_avaliable.size())
  {
    if (m_resource_error)
      break;
    int retcode = pthread_cond_timedwait(&m_input_buffer_cond, &m_omx_input_mutex, &endtime);
    if (retcode != 0) {
      if (timeout != 0)
        CLogLog(LOGERROR, "COMXCoreComponent::WaitForInputDone %s wait event timeout", m_componentName);
      omx_err = OMX_ErrorTimeout;
      break;
    }
  }
  pthread_mutex_unlock(&m_omx_input_mutex);
  return omx_err;
}


OMX_ERRORTYPE COMXCoreComponent::WaitForOutputDone(long timeout /*=200*/)
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  pthread_mutex_lock(&m_omx_output_mutex);
  struct timespec endtime;
  clock_gettime(CLOCK_REALTIME, &endtime);
  add_timespecs(endtime, timeout);
  while (m_output_buffer_count != m_omx_output_available.size())
  {
    if (m_resource_error)
      break;
    int retcode = pthread_cond_timedwait(&m_output_buffer_cond, &m_omx_output_mutex, &endtime);
    if (retcode != 0) {
      if (timeout != 0)
        CLogLog(LOGERROR, "COMXCoreComponent::WaitForOutputDone %s wait event timeout", m_componentName);
      omx_err = OMX_ErrorTimeout;
      break;
    }
  }
  pthread_mutex_unlock(&m_omx_output_mutex);
  return omx_err;
}


OMX_ERRORTYPE COMXCoreComponent::AllocInputBuffers()
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  if(!m_handle)
    return OMX_ErrorUndefined;

  OMX_PARAM_PORTDEFINITIONTYPE portFormat;
  OMX_INIT_STRUCTURE(portFormat);
  portFormat.nPortIndex = m_input_port;

  omx_err = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portFormat);
  if(omx_err != OMX_ErrorNone)
    return omx_err;

  if(GetState() != OMX_StateIdle)
  {
    if(GetState() != OMX_StateLoaded)
      SetStateForComponent(OMX_StateLoaded);

    SetStateForComponent(OMX_StateIdle);
  }

  omx_err = EnablePort(m_input_port, false);
  if(omx_err != OMX_ErrorNone)
    return omx_err;

  m_input_alignment     = portFormat.nBufferAlignment;
  m_input_buffer_count  = portFormat.nBufferCountActual;
  m_input_buffer_size   = portFormat.nBufferSize;

  CLogLog(LOGDEBUG, "COMXCoreComponent::AllocInputBuffers component(%s) - port(%d), nBufferCountMin(%u), nBufferCountActual(%u), nBufferSize(%u), nBufferAlignmen(%u)",
            m_componentName, GetInputPort(), portFormat.nBufferCountMin,
            portFormat.nBufferCountActual, portFormat.nBufferSize, portFormat.nBufferAlignment);

  for (size_t i = 0; i < portFormat.nBufferCountActual; i++)
  {
    OMX_BUFFERHEADERTYPE *buffer = NULL;

    omx_err = OMX_AllocateBuffer(m_handle, &buffer, m_input_port, NULL, portFormat.nBufferSize);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreComponent::AllocInputBuffers component(%s) - OMX_UseBuffer failed with omx_err(0x%x)",
        m_componentName, omx_err);

      return omx_err;
    }
    buffer->nInputPortIndex = m_input_port;
    buffer->nFilledLen      = 0;
    buffer->nOffset         = 0;
    buffer->pAppPrivate     = (void*)i;  
    m_omx_input_buffers.push_back(buffer);
    m_omx_input_avaliable.push(buffer);
  }

  omx_err = WaitForCommand(OMX_CommandPortEnable, m_input_port);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::AllocInputBuffers WaitForCommand:OMX_CommandPortEnable failed on %s omx_err(0x%08x)", m_componentName, omx_err);
    return omx_err;
  }

  m_flush_input = false;

  return omx_err;
}

OMX_ERRORTYPE COMXCoreComponent::FreeInputBuffers()
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  if(!m_handle)
    return OMX_ErrorUndefined;

  if(m_omx_input_buffers.empty())
    return OMX_ErrorNone;

  m_flush_input = true;

  omx_err = DisablePort(m_input_port, false);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::FreeInputBuffers failed to disable port on %s omx_err(0x%08x)", m_componentName, omx_err);
  }

  pthread_mutex_lock(&m_omx_input_mutex);
  pthread_cond_broadcast(&m_input_buffer_cond);

  for (size_t i = 0; i < m_omx_input_buffers.size(); i++)
  {
    omx_err = OMX_FreeBuffer(m_handle, m_input_port, m_omx_input_buffers[i]);

    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreComponent::FreeInputBuffers error deallocate omx input buffer on component %s omx_err(0x%08x)", m_componentName, omx_err);
    }
  }
  pthread_mutex_unlock(&m_omx_input_mutex);

  omx_err = WaitForCommand(OMX_CommandPortDisable, m_input_port);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::FreeInputBuffers WaitForCommand:OMX_CommandPortDisable failed on %s omx_err(0x%08x)", m_componentName, omx_err);
  }

  WaitForInputDone(1000);

  pthread_mutex_lock(&m_omx_input_mutex);
  assert(m_omx_input_buffers.size() == m_omx_input_avaliable.size());

  m_omx_input_buffers.clear();

  while (!m_omx_input_avaliable.empty())
    m_omx_input_avaliable.pop();

  m_input_alignment     = 0;
  m_input_buffer_size   = 0;
  m_input_buffer_count  = 0;

  pthread_mutex_unlock(&m_omx_input_mutex);

  return omx_err;
}

OMX_ERRORTYPE COMXCoreComponent::FreeOutputBuffers()
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  if(!m_handle)
    return OMX_ErrorUndefined;

  if(m_omx_output_buffers.empty())
    return OMX_ErrorNone;

  m_flush_output = true;

  omx_err = DisablePort(m_output_port, false);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::FreeOutputBuffers failed to disable port on %s omx_err(0x%08x)", m_componentName, omx_err);
  }

  pthread_mutex_lock(&m_omx_output_mutex);
  pthread_cond_broadcast(&m_output_buffer_cond);

  for (size_t i = 0; i < m_omx_output_buffers.size(); i++)
  {
    omx_err = OMX_FreeBuffer(m_handle, m_output_port, m_omx_output_buffers[i]);

    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreComponent::FreeOutputBuffers error deallocate omx output buffer on component %s omx_err(0x%08x)", m_componentName, omx_err);
    }
  }
  pthread_mutex_unlock(&m_omx_output_mutex);

  omx_err = WaitForCommand(OMX_CommandPortDisable, m_output_port);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::FreeOutputBuffers WaitForCommand:OMX_CommandPortDisable failed on %s omx_err(0x%08x)", m_componentName, omx_err);
  }

  WaitForOutputDone(1000);

  pthread_mutex_lock(&m_omx_output_mutex);
  assert(m_omx_output_buffers.size() == m_omx_output_available.size());

  m_omx_output_buffers.clear();

  while (!m_omx_output_available.empty())
    m_omx_output_available.pop();

  m_output_alignment    = 0;
  m_output_buffer_count = 0;

  pthread_mutex_unlock(&m_omx_output_mutex);

  return omx_err;
}

OMX_ERRORTYPE COMXCoreComponent::DisableAllPorts()
{
  if(!m_handle)
    return OMX_ErrorUndefined;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  OMX_INDEXTYPE idxTypes[] = {
    OMX_IndexParamAudioInit,
    OMX_IndexParamImageInit,
    OMX_IndexParamVideoInit, 
    OMX_IndexParamOtherInit
  };

  OMX_PORT_PARAM_TYPE ports;
  OMX_INIT_STRUCTURE(ports);

  int i;
  for(i=0; i < 4; i++)
  {
    omx_err = OMX_GetParameter(m_handle, idxTypes[i], &ports);
    if(omx_err == OMX_ErrorNone) {

      uint32_t j;
      for(j=0; j<ports.nPorts; j++)
      {
        OMX_PARAM_PORTDEFINITIONTYPE portFormat;
        OMX_INIT_STRUCTURE(portFormat);
        portFormat.nPortIndex = ports.nStartPortNumber+j;

        omx_err = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portFormat);
        if(omx_err != OMX_ErrorNone)
        {
          if(portFormat.bEnabled == OMX_FALSE)
            continue;
        }

        omx_err = OMX_SendCommand(m_handle, OMX_CommandPortDisable, ports.nStartPortNumber+j, NULL);
        if(omx_err != OMX_ErrorNone)
        {
          CLogLog(LOGERROR, "COMXCoreComponent::DisableAllPorts - Error disable port %d on component %s omx_err(0x%08x)",
            (int)(ports.nStartPortNumber) + j, m_componentName, (int)omx_err);
        }
        omx_err = WaitForCommand(OMX_CommandPortDisable, ports.nStartPortNumber+j);
        if(omx_err != OMX_ErrorNone && omx_err != OMX_ErrorSameState)
          return omx_err;
      }
    }
  }

  return OMX_ErrorNone;
}

void COMXCoreComponent::RemoveEvent(OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2)
{
  for (std::vector<omx_event>::iterator it = m_omx_events.begin(); it != m_omx_events.end(); ) 
  {
    omx_event event = *it;

    if(event.eEvent == eEvent && event.nData1 == nData1 && event.nData2 == nData2) 
    {
      it = m_omx_events.erase(it);
      continue;
    }
    ++it;
  }
}

OMX_ERRORTYPE COMXCoreComponent::AddEvent(OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2)
{
  omx_event event;

  event.eEvent      = eEvent;
  event.nData1      = nData1;
  event.nData2      = nData2;

  pthread_mutex_lock(&m_omx_event_mutex);
  RemoveEvent(eEvent, nData1, nData2);
  m_omx_events.push_back(event);
  // this allows (all) blocked tasks to be awoken
  pthread_cond_broadcast(&m_omx_event_cond);
  pthread_mutex_unlock(&m_omx_event_mutex);

#ifdef OMX_DEBUG_EVENTS
  CLogLog(LOGDEBUG, "COMXCoreComponent::AddEvent %s add event event.eEvent 0x%08x event.nData1 0x%08x event.nData2 %d",
          m_componentName, (int)event.eEvent, (int)event.nData1, (int)event.nData2);
#endif

  return OMX_ErrorNone;
}

// timeout in milliseconds
OMX_ERRORTYPE COMXCoreComponent::WaitForEvent(OMX_EVENTTYPE eventType, long timeout)
{
#ifdef OMX_DEBUG_EVENTS
  CLogLog(LOGDEBUG, "COMXCoreComponent::WaitForEvent %s wait event 0x%08x",
      m_componentName, (int)eventType);
#endif

  pthread_mutex_lock(&m_omx_event_mutex);
  struct timespec endtime;
  clock_gettime(CLOCK_REALTIME, &endtime);
  add_timespecs(endtime, timeout);
  while(true) 
  {
    for (std::vector<omx_event>::iterator it = m_omx_events.begin(); it != m_omx_events.end(); ++it)
    {
      omx_event event = *it;

#ifdef OMX_DEBUG_EVENTS
      CLogLog(LOGDEBUG, "COMXCoreComponent::WaitForEvent %s inlist event event.eEvent 0x%08x event.nData1 0x%08x event.nData2 %d",
          m_componentName, (int)event.eEvent, (int)event.nData1, (int)event.nData2);
#endif


      if(event.eEvent == OMX_EventError && event.nData1 == (OMX_U32)OMX_ErrorSameState && event.nData2 == 1)
      {
#ifdef OMX_DEBUG_EVENTS
        CLogLog(LOGDEBUG, "COMXCoreComponent::WaitForEvent %s remove event event.eEvent 0x%08x event.nData1 0x%08x event.nData2 %d",
          m_componentName, (int)event.eEvent, (int)event.nData1, (int)event.nData2);
#endif
        m_omx_events.erase(it);
        pthread_mutex_unlock(&m_omx_event_mutex);
        return OMX_ErrorNone;
      }
      else if(event.eEvent == OMX_EventError) 
      {
        m_omx_events.erase(it);
        pthread_mutex_unlock(&m_omx_event_mutex);
        return (OMX_ERRORTYPE)event.nData1;
      }
      else if(event.eEvent == eventType) 
      {
#ifdef OMX_DEBUG_EVENTS
        CLogLog(LOGDEBUG, "COMXCoreComponent::WaitForEvent %s remove event event.eEvent 0x%08x event.nData1 0x%08x event.nData2 %d",
          m_componentName, (int)event.eEvent, (int)event.nData1, (int)event.nData2);
#endif

        m_omx_events.erase(it);
        pthread_mutex_unlock(&m_omx_event_mutex);
        return OMX_ErrorNone;
      }
    }

    if (m_resource_error)
      break;
    int retcode = pthread_cond_timedwait(&m_omx_event_cond, &m_omx_event_mutex, &endtime);
    if (retcode != 0) 
    {
      if (timeout > 0)
        CLogLog(LOGERROR, "COMXCoreComponent::WaitForEvent %s wait event 0x%08x timeout %ld",
                          m_componentName, (int)eventType, timeout);
      pthread_mutex_unlock(&m_omx_event_mutex);
      return OMX_ErrorTimeout;
    }
  }
  pthread_mutex_unlock(&m_omx_event_mutex);
  return OMX_ErrorNone;
}

// timeout in milliseconds
OMX_ERRORTYPE COMXCoreComponent::WaitForCommand(OMX_U32 command, OMX_U32 nData2, long timeout)
{
#ifdef OMX_DEBUG_EVENTS
  CLogLog(LOGDEBUG, "COMXCoreComponent::WaitForCommand %s wait event.eEvent 0x%08x event.command 0x%08x event.nData2 %d",
      m_componentName, (int)OMX_EventCmdComplete, (int)command, (int)nData2);
#endif

  pthread_mutex_lock(&m_omx_event_mutex);
  struct timespec endtime;
  clock_gettime(CLOCK_REALTIME, &endtime);
  add_timespecs(endtime, timeout);
  while(true) 
  {
    for (std::vector<omx_event>::iterator it = m_omx_events.begin(); it != m_omx_events.end(); ++it)
    {
      omx_event event = *it;

#ifdef OMX_DEBUG_EVENTS
      CLogLog(LOGDEBUG, "COMXCoreComponent::WaitForCommand %s inlist event event.eEvent 0x%08x event.nData1 0x%08x event.nData2 %d",
          m_componentName, (int)event.eEvent, (int)event.nData1, (int)event.nData2);
#endif
      if(event.eEvent == OMX_EventError && event.nData1 == (OMX_U32)OMX_ErrorSameState && event.nData2 == 1)
      {
#ifdef OMX_DEBUG_EVENTS
        CLogLog(LOGDEBUG, "COMXCoreComponent::WaitForCommand %s remove event event.eEvent 0x%08x event.nData1 0x%08x event.nData2 %d",
          m_componentName, (int)event.eEvent, (int)event.nData1, (int)event.nData2);
#endif

        m_omx_events.erase(it);
        pthread_mutex_unlock(&m_omx_event_mutex);
        return OMX_ErrorNone;
      } 
      else if(event.eEvent == OMX_EventError) 
      {
        m_omx_events.erase(it);
        pthread_mutex_unlock(&m_omx_event_mutex);
        return (OMX_ERRORTYPE)event.nData1;
      } 
      else if(event.eEvent == OMX_EventCmdComplete && event.nData1 == command && event.nData2 == nData2) 
      {

#ifdef OMX_DEBUG_EVENTS
        CLogLog(LOGDEBUG, "COMXCoreComponent::WaitForCommand %s remove event event.eEvent 0x%08x event.nData1 0x%08x event.nData2 %d",
          m_componentName, (int)event.eEvent, (int)event.nData1, (int)event.nData2);
#endif

        m_omx_events.erase(it);
        pthread_mutex_unlock(&m_omx_event_mutex);
        return OMX_ErrorNone;
      }
    }

    if (m_resource_error)
      break;
    int retcode = pthread_cond_timedwait(&m_omx_event_cond, &m_omx_event_mutex, &endtime);
    if (retcode != 0) {
      CLogLog(LOGERROR, "COMXCoreComponent::WaitForCommand %s wait timeout event.eEvent 0x%08x event.command 0x%08x event.nData2 %d",
        m_componentName, (int)OMX_EventCmdComplete, (int)command, (int)nData2);
      
      pthread_mutex_unlock(&m_omx_event_mutex);
      return OMX_ErrorTimeout;
    }
  }
  pthread_mutex_unlock(&m_omx_event_mutex);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE COMXCoreComponent::SetStateForComponent(OMX_STATETYPE state)
{
  if(!m_handle)
    return OMX_ErrorUndefined;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_STATETYPE state_actual = OMX_StateMax;

  if(state == state_actual)
    return OMX_ErrorNone;

  omx_err = OMX_SendCommand(m_handle, OMX_CommandStateSet, state, 0);
  if (omx_err != OMX_ErrorNone)
  {
    if(omx_err == OMX_ErrorSameState)
    {
      CLogLog(LOGERROR, "COMXCoreComponent::SetStateForComponent - %s same state",
        m_componentName);
      omx_err = OMX_ErrorNone;
    }
    else
    {
      CLogLog(LOGERROR, "COMXCoreComponent::SetStateForComponent - %s failed with omx_err(0x%x)",
        m_componentName, omx_err);
    }
  }
  else 
  {
    omx_err = WaitForCommand(OMX_CommandStateSet, state);
    if (omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreComponent::WaitForCommand - %s failed with omx_err(0x%x)",
        m_componentName, omx_err);
    }
  }
  return omx_err;
}

OMX_STATETYPE COMXCoreComponent::GetState() const
{
  if(!m_handle)
    return (OMX_STATETYPE)0;

  OMX_STATETYPE state;

  OMX_ERRORTYPE omx_err = OMX_GetState(m_handle, &state);
  if (omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::GetState - %s failed with omx_err(0x%x)",
      m_componentName, omx_err);
  }
  return state;
}

OMX_ERRORTYPE COMXCoreComponent::SetParameter(OMX_INDEXTYPE paramIndex, OMX_PTR paramStruct)
{
  if(!m_handle)
    return OMX_ErrorUndefined;

  OMX_ERRORTYPE omx_err;

  omx_err = OMX_SetParameter(m_handle, paramIndex, paramStruct);
  if(omx_err != OMX_ErrorNone) 
  {
    CLogLog(LOGERROR, "COMXCoreComponent::SetParameter - %s failed with omx_err(0x%x)",
              m_componentName, omx_err);
  }
  return omx_err;
}

OMX_ERRORTYPE COMXCoreComponent::GetParameter(OMX_INDEXTYPE paramIndex, OMX_PTR paramStruct) const
{
  if(!m_handle)
    return OMX_ErrorUndefined;

  OMX_ERRORTYPE omx_err;

  omx_err = OMX_GetParameter(m_handle, paramIndex, paramStruct);
  if(omx_err != OMX_ErrorNone) 
  {
    CLogLog(LOGERROR, "COMXCoreComponent::GetParameter - %s failed with omx_err(0x%x)",
              m_componentName, omx_err);
  }
  return omx_err;
}

OMX_ERRORTYPE COMXCoreComponent::SetConfig(OMX_INDEXTYPE configIndex, OMX_PTR configStruct)
{
  if(!m_handle)
    return OMX_ErrorUndefined;

  OMX_ERRORTYPE omx_err;

  omx_err = OMX_SetConfig(m_handle, configIndex, configStruct);
  if(omx_err != OMX_ErrorNone) 
  {
    CLogLog(LOGERROR, "COMXCoreComponent::SetConfig - %s failed with omx_err(0x%x)",
              m_componentName, omx_err);
  }
  return omx_err;
}

OMX_ERRORTYPE COMXCoreComponent::GetConfig(OMX_INDEXTYPE configIndex, OMX_PTR configStruct) const
{
  if(!m_handle)
    return OMX_ErrorUndefined;

  OMX_ERRORTYPE omx_err;

  omx_err = OMX_GetConfig(m_handle, configIndex, configStruct);
  if(omx_err != OMX_ErrorNone) 
  {
    CLogLog(LOGERROR, "COMXCoreComponent::GetConfig - %s failed with omx_err(0x%x)",
              m_componentName, omx_err);
  }
  return omx_err;
}

OMX_ERRORTYPE COMXCoreComponent::EnablePort(unsigned int port,  bool wait)
{
  if(!m_handle)
    return OMX_ErrorUndefined;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  OMX_PARAM_PORTDEFINITIONTYPE portFormat;
  OMX_INIT_STRUCTURE(portFormat);
  portFormat.nPortIndex = port;

  omx_err = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portFormat);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::EnablePort - Error get port %d status on component %s omx_err(0x%08x)",
        port, m_componentName, (int)omx_err);
  }

  if(portFormat.bEnabled == OMX_FALSE)
  {
    omx_err = OMX_SendCommand(m_handle, OMX_CommandPortEnable, port, NULL);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreComponent::EnablePort - Error enable port %d on component %s omx_err(0x%08x)",
          port, m_componentName, (int)omx_err);
        return omx_err;
    }
    else
    {
      if(wait)
        omx_err = WaitForCommand(OMX_CommandPortEnable, port);
    }
  }
  return omx_err;
}

OMX_ERRORTYPE COMXCoreComponent::DisablePort(unsigned int port, bool wait)
{
  if(!m_handle)
    return OMX_ErrorUndefined;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  OMX_PARAM_PORTDEFINITIONTYPE portFormat;
  OMX_INIT_STRUCTURE(portFormat);
  portFormat.nPortIndex = port;

  omx_err = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portFormat);
  if(omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::DisablePort - Error get port %d status on component %s omx_err(0x%08x)",
        port, m_componentName, (int)omx_err);
  }

  if(portFormat.bEnabled == OMX_TRUE)
  {
    omx_err = OMX_SendCommand(m_handle, OMX_CommandPortDisable, port, NULL);
    if(omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreComponent::DIsablePort - Error disable port %d on component %s omx_err(0x%08x)",
          port, m_componentName, (int)omx_err);
      return omx_err;
    }
    else
    {
      if(wait)
        omx_err = WaitForCommand(OMX_CommandPortDisable, port);
    }
  }
  return omx_err;
}

bool COMXCoreComponent::Initialize(const char *component_name, OMX_INDEXTYPE index)
{
  OMX_ERRORTYPE omx_err;

  m_input_port  = 0;
  m_output_port = 0;
  m_handle      = NULL;

  m_input_alignment     = 0;
  m_input_buffer_size  = 0;
  m_input_buffer_count  = 0;

  m_output_alignment    = 0;
  m_output_buffer_count = 0;
  m_flush_input         = false;
  m_flush_output        = false;
  m_resource_error      = false;

  m_eos                 = false;

  m_exit = false;

  m_omx_events.clear();
  m_ignore_error = OMX_ErrorNone;

  m_componentName = component_name;
  
  m_callbacks.EventHandler    = &COMXCoreComponent::DecoderEventHandlerCallback;
  m_callbacks.EmptyBufferDone = &COMXCoreComponent::DecoderEmptyBufferDoneCallback;
  m_callbacks.FillBufferDone  = &COMXCoreComponent::DecoderFillBufferDoneCallback;

  // Get video component handle setting up callbacks, component is in loaded state on return.
  if(!m_handle)
  {
    if (strncmp("OMX.alsa.", component_name, 9) == 0)
      omx_err = OMXALSA_GetHandle(&m_handle, (char*) component_name, this, &m_callbacks);
    else
      omx_err = OMX_GetHandle(&m_handle, (char*)component_name, this, &m_callbacks);

    if (!m_handle || omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreComponent::Initialize - could not get component handle for %s omx_err(0x%08x)",
          component_name, (int)omx_err);
      Deinitialize();
      return false;
    }
  }

  OMX_PORT_PARAM_TYPE port_param;
  OMX_INIT_STRUCTURE(port_param);

  omx_err = OMX_GetParameter(m_handle, index, &port_param);
  if (omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::Initialize - could not get port_param for component %s omx_err(0x%08x)",
        component_name, (int)omx_err);
  }

  omx_err = DisableAllPorts();
  if (omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCoreComponent::Initialize - error disable ports on component %s omx_err(0x%08x)",
        component_name, (int)omx_err);
  }

  m_input_port  = port_param.nStartPortNumber;
  m_output_port = m_input_port + 1;

  if(strcmp(m_componentName, "OMX.broadcom.audio_mixer") == 0)
  {
    m_input_port  = port_param.nStartPortNumber + 1;
    m_output_port = port_param.nStartPortNumber;
  }

  if (m_output_port > port_param.nStartPortNumber+port_param.nPorts-1)
    m_output_port = port_param.nStartPortNumber+port_param.nPorts-1;

  CLogLog(LOGDEBUG, "COMXCoreComponent::Initialize %s input port %d output port %d m_handle %p",
      m_componentName, m_input_port, m_output_port, m_handle);

  m_exit = false;
  m_flush_input   = false;
  m_flush_output  = false;

  return true;
}

void COMXCoreComponent::ResetEos()
{
  pthread_mutex_lock(&m_omx_eos_mutex);
  m_eos = false;
  pthread_mutex_unlock(&m_omx_eos_mutex);
}

bool COMXCoreComponent::Deinitialize()
{
  OMX_ERRORTYPE omx_err;

  m_exit = true;

  m_flush_input   = true;
  m_flush_output  = true;

  if(m_handle)
  {
    FlushAll();

    FreeOutputBuffers();
    FreeInputBuffers();

    TransitionToStateLoaded();

    CLogLog(LOGDEBUG, "COMXCoreComponent::Deinitialize : %s handle %p",
        m_componentName, m_handle);
#ifdef TARGET_LINUX
    if (strncmp("OMX.alsa.", m_componentName, 9) == 0)
      omx_err = OMXALSA_FreeHandle(m_handle);
    else
#endif
    omx_err = OMX_FreeHandle(m_handle);
    if (omx_err != OMX_ErrorNone)
    {
      CLogLog(LOGERROR, "COMXCoreComponent::Deinitialize - failed to free handle for component %s omx_err(0x%08x)",
          m_componentName, omx_err);
    }
    m_handle = NULL;

    m_input_port      = 0;
    m_output_port     = 0;
    m_componentName   = NULL;
    m_resource_error  = false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////
// DecoderEventHandler -- OMX event callback
OMX_ERRORTYPE COMXCoreComponent::DecoderEventHandlerCallback(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 nData1,
  OMX_U32 nData2,
  OMX_PTR pEventData)
{
  if(!pAppData)
    return OMX_ErrorNone;

  COMXCoreComponent *ctx = static_cast<COMXCoreComponent*>(pAppData);
  return ctx->DecoderEventHandler(hComponent, eEvent, nData1, nData2, pEventData);
}

// DecoderEmptyBufferDone -- OMXCore input buffer has been emptied
OMX_ERRORTYPE COMXCoreComponent::DecoderEmptyBufferDoneCallback(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
  if(!pAppData)
    return OMX_ErrorNone;

  COMXCoreComponent *ctx = static_cast<COMXCoreComponent*>(pAppData);
  return ctx->DecoderEmptyBufferDone( hComponent, pBuffer);
}

// DecoderFillBufferDone -- OMXCore output buffer has been filled
OMX_ERRORTYPE COMXCoreComponent::DecoderFillBufferDoneCallback(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
  if(!pAppData)
    return OMX_ErrorNone;

  COMXCoreComponent *ctx = static_cast<COMXCoreComponent*>(pAppData);
  return ctx->DecoderFillBufferDone(hComponent, pBuffer);
}

OMX_ERRORTYPE COMXCoreComponent::DecoderEmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE* pBuffer)
{
  if(m_exit)
    return OMX_ErrorNone;

  #if defined(OMX_DEBUG_EVENTHANDLER)
  CLogLog(LOGDEBUG, "COMXCoreComponent::DecoderEmptyBufferDone component(%s) %p %d/%d", m_componentName, pBuffer, m_omx_input_avaliable.size(), m_input_buffer_count);
  #endif
  pthread_mutex_lock(&m_omx_input_mutex);
  m_omx_input_avaliable.push(pBuffer);

  // this allows (all) blocked tasks to be awoken
  pthread_cond_broadcast(&m_input_buffer_cond);

  pthread_mutex_unlock(&m_omx_input_mutex);

  return OMX_ErrorNone;
}

OMX_ERRORTYPE COMXCoreComponent::DecoderFillBufferDone(OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE* pBuffer)
{
  if(m_exit)
    return OMX_ErrorNone;

  #if defined(OMX_DEBUG_EVENTHANDLER)
  CLogLog(LOGDEBUG, "COMXCoreComponent::DecoderFillBufferDone component(%s) %p %d/%d", m_componentName, pBuffer, m_omx_output_available.size(), m_output_buffer_count);
  #endif
  pthread_mutex_lock(&m_omx_output_mutex);
  m_omx_output_available.push(pBuffer);

  // this allows (all) blocked tasks to be awoken
  pthread_cond_broadcast(&m_output_buffer_cond);

  pthread_mutex_unlock(&m_omx_output_mutex);

  return OMX_ErrorNone;
}

// DecoderEmptyBufferDone -- OMXCore input buffer has been emptied
////////////////////////////////////////////////////////////////////////////////////////////
// Component event handler -- OMX event callback
OMX_ERRORTYPE COMXCoreComponent::DecoderEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_EVENTTYPE eEvent,
  OMX_U32 nData1,
  OMX_U32 nData2,
  OMX_PTR pEventData)
{
#ifdef OMX_DEBUG_EVENTS
  CLogLog(LOGDEBUG,
    "COMXCoreComponent::%s - %s eEvent(0x%x), nData1(0x%x), nData2(0x%x), pEventData(0x%p)",
    __func__, GetName(), eEvent, nData1, nData2, pEventData);
#endif

  // if the error is expected, then we can skip it
  if (eEvent == OMX_EventError && (OMX_S32)nData1 == m_ignore_error)
  {
    CLogLog(LOGDEBUG,
      "COMXCoreComponent::%s - %s Ignoring expected event: eEvent(0x%x), nData1(0x%x), nData2(0x%x), pEventData(0x%p)",
      __func__, GetName(), eEvent, nData1, nData2, pEventData);
    m_ignore_error = OMX_ErrorNone;
    return OMX_ErrorNone;
  }
  AddEvent(eEvent, nData1, nData2);

  switch (eEvent)
  {
    case OMX_EventCmdComplete:
      
      switch(nData1)
      {
        case OMX_CommandStateSet:
          switch ((int)nData2)
          {
            case OMX_StateInvalid:
            #if defined(OMX_DEBUG_EVENTHANDLER)
              CLogLog(LOGDEBUG, "%s::%s %s - OMX_StateInvalid", CLASSNAME, __func__, GetName());
            #endif
            break;
            case OMX_StateLoaded:
            #if defined(OMX_DEBUG_EVENTHANDLER)
              CLogLog(LOGDEBUG, "%s::%s %s - OMX_StateLoaded", CLASSNAME, __func__, GetName());
            #endif
            break;
            case OMX_StateIdle:
            #if defined(OMX_DEBUG_EVENTHANDLER)
              CLogLog(LOGDEBUG, "%s::%s %s - OMX_StateIdle", CLASSNAME, __func__, GetName());
            #endif
            break;
            case OMX_StateExecuting:
            #if defined(OMX_DEBUG_EVENTHANDLER)
              CLogLog(LOGDEBUG, "%s::%s %s - OMX_StateExecuting", CLASSNAME, __func__, GetName());
            #endif
            break;
            case OMX_StatePause:
            #if defined(OMX_DEBUG_EVENTHANDLER)
              CLogLog(LOGDEBUG, "%s::%s %s - OMX_StatePause", CLASSNAME, __func__, GetName());
            #endif
            break;
            case OMX_StateWaitForResources:
            #if defined(OMX_DEBUG_EVENTHANDLER)
              CLogLog(LOGDEBUG, "%s::%s %s - OMX_StateWaitForResources", CLASSNAME, __func__, GetName());
            #endif
            break;
            default:
            #if defined(OMX_DEBUG_EVENTHANDLER)
              CLogLog(LOGDEBUG,
                "%s::%s %s - Unknown OMX_Statexxxxx, state(%d)", CLASSNAME, __func__, GetName(), (int)nData2);
            #endif
            break;
          }
        break;
        case OMX_CommandFlush:
          #if defined(OMX_DEBUG_EVENTHANDLER)
          CLogLog(LOGDEBUG, "%s::%s %s - OMX_CommandFlush, port %d", CLASSNAME, __func__, GetName(), (int)nData2);
          #endif
        break;
        case OMX_CommandPortDisable:
          #if defined(OMX_DEBUG_EVENTHANDLER)
          CLogLog(LOGDEBUG, "%s::%s %s - OMX_CommandPortDisable, nData1(0x%x), port %d", CLASSNAME, __func__, GetName(), nData1, (int)nData2);
          #endif
        break;
        case OMX_CommandPortEnable:
          #if defined(OMX_DEBUG_EVENTHANDLER)
          CLogLog(LOGDEBUG, "%s::%s %s - OMX_CommandPortEnable, nData1(0x%x), port %d", CLASSNAME, __func__, GetName(), nData1, (int)nData2);
          #endif
        break;
        #if defined(OMX_DEBUG_EVENTHANDLER)
        case OMX_CommandMarkBuffer:
          CLogLog(LOGDEBUG, "%s::%s %s - OMX_CommandMarkBuffer, nData1(0x%x), port %d", CLASSNAME, __func__, GetName(), nData1, (int)nData2);
        break;
        #endif
      }
    break;
    case OMX_EventBufferFlag:
      #if defined(OMX_DEBUG_EVENTHANDLER)
      CLogLog(LOGDEBUG, "%s::%s %s - OMX_EventBufferFlag(input)", CLASSNAME, __func__, GetName());
      #endif
      if(nData2 & OMX_BUFFERFLAG_EOS)
      {
        pthread_mutex_lock(&m_omx_eos_mutex);
        m_eos = true;
        pthread_mutex_unlock(&m_omx_eos_mutex);
      }
    break;
    case OMX_EventPortSettingsChanged:
      #if defined(OMX_DEBUG_EVENTHANDLER)
      CLogLog(LOGDEBUG, "%s::%s %s - OMX_EventPortSettingsChanged(output)", CLASSNAME, __func__, GetName());
      #endif
    break;
    case OMX_EventParamOrConfigChanged:
      #if defined(OMX_DEBUG_EVENTHANDLER)
      CLogLog(LOGDEBUG, "%s::%s %s - OMX_EventParamOrConfigChanged(output)", CLASSNAME, __func__, GetName());
      #endif
    break;
    #if defined(OMX_DEBUG_EVENTHANDLER)
    case OMX_EventMark:
      CLogLog(LOGDEBUG, "%s::%s %s - OMX_EventMark", CLASSNAME, __func__, GetName());
    break;
    case OMX_EventResourcesAcquired:
      CLogLog(LOGDEBUG, "%s::%s %s- OMX_EventResourcesAcquired", CLASSNAME, __func__, GetName());
    break;
    #endif
    case OMX_EventError:
      switch((OMX_S32)nData1)
      {
        case OMX_ErrorSameState:
          //#if defined(OMX_DEBUG_EVENTHANDLER)
          //CLogLog(LOGERROR, "%s::%s %s - OMX_ErrorSameState, same state", CLASSNAME, __func__, GetName());
          //#endif
        break;
        case OMX_ErrorInsufficientResources:
          CLogLog(LOGERROR, "%s::%s %s - OMX_ErrorInsufficientResources, insufficient resources", CLASSNAME, __func__, GetName());
          m_resource_error = true;
        break;
        case OMX_ErrorFormatNotDetected:
          CLogLog(LOGERROR, "%s::%s %s - OMX_ErrorFormatNotDetected, cannot parse input stream", CLASSNAME, __func__, GetName());
        break;
        case OMX_ErrorPortUnpopulated:
        CLogLog(LOGWARNING, "%s::%s %s - OMX_ErrorPortUnpopulated port %d", CLASSNAME, __func__, GetName(), (int)nData2);
        break;
        case OMX_ErrorStreamCorrupt:
          CLogLog(LOGERROR, "%s::%s %s - OMX_ErrorStreamCorrupt, Bitstream corrupt", CLASSNAME, __func__, GetName());
          m_resource_error = true;
        break;
        case OMX_ErrorUnsupportedSetting:
          CLogLog(LOGERROR, "%s::%s %s - OMX_ErrorUnsupportedSetting, unsupported setting", CLASSNAME, __func__, GetName());
        break;
        default:
          CLogLog(LOGERROR, "%s::%s %s - OMX_EventError detected, nData1(0x%x), port %d",  CLASSNAME, __func__, GetName(), nData1, (int)nData2);
        break;
      }
      // wake things up
      if (m_resource_error)
      {
        pthread_cond_broadcast(&m_output_buffer_cond);
        pthread_cond_broadcast(&m_input_buffer_cond);
        pthread_cond_broadcast(&m_omx_event_cond);
      }
    break;
    default:
      CLogLog(LOGWARNING, "%s::%s %s - Unknown eEvent(0x%x), nData1(0x%x), port %d", CLASSNAME, __func__, GetName(), eEvent, nData1, (int)nData2);
    break;
  }

  return OMX_ErrorNone;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
COMXCore::COMXCore()
{
  OMX_ERRORTYPE omx_err = OMX_Init();
  if (omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCore::Initialize - OMXCore failed to init, omx_err(0x%08x)", omx_err);
    throw "OMXCore failed to init";
  }
}

COMXCore::~COMXCore()
{
  OMX_ERRORTYPE omx_err = OMX_Deinit();
  if (omx_err != OMX_ErrorNone)
  {
    CLogLog(LOGERROR, "COMXCore::Deinitialize - OMXCore failed to deinit, omx_err(0x%08x)", omx_err);
  }
}
