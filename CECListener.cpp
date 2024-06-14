/*
 *
 *      Copyright (C) 2022 Michael Walsh
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "KeyConfig.h"
#include "CECListener.h"

extern "C" {
#include <bcm_host.h>
};


CECListener::CECListener()
{
  vc_cec_set_osd_name("OMXPlayer");
  vc_cec_set_passive(true);

  CEC_AllDevices_T logaddr;
  vc_cec_get_logical_address(&logaddr);
  if(logaddr == 0xF)
  {
    vc_cec_alloc_logical_address();
    vc_cec_register_callback(InitCallback, this);
  }
  else
  {
    vc_cec_register_callback(ActionCallback, this);
  }
}

void CECListener::InitCallback(void *object, uint32_t response, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4)
{
  if(CEC_CB_REASON(response) != VC_CEC_LOGICAL_ADDR) return;

  vc_cec_send_ReportPhysicalAddress(param2, CEC_DeviceType_Playback, false);
  vc_cec_register_callback(ActionCallback, object);
}

void CECListener::ActionCallback(void *object, uint32_t response, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4)
{
  if(CEC_CB_REASON(response) != VC_CEC_BUTTON_PRESSED) return;

  CECListener *listener = static_cast<CECListener*>(object);
  int cec_buttoncode = CEC_CB_OPERAND1(param1);

  switch (cec_buttoncode)
  {
  case CEC_User_Control_Up:
    listener->m_action = ACTION_NEXT_FILE;
    break;
  case CEC_User_Control_Down:
    listener->m_action = ACTION_PREVIOUS_FILE;
    break;
  case CEC_User_Control_Left:
    listener->m_action = ACTION_PREVIOUS_CHAPTER;
    break;
  case CEC_User_Control_Right:
    listener->m_action = ACTION_NEXT_CHAPTER;
    break;
  case CEC_User_Control_Exit:
  case CEC_User_Control_Stop:
    listener->m_action = ACTION_EXIT;
    break;
  case CEC_User_Control_SoundSelect:
  case CEC_User_Control_F3Green:
    listener->m_action = ACTION_NEXT_AUDIO;
    break;
  case CEC_User_Control_Play:
    listener->m_action = ACTION_PLAY;
    break;
  case CEC_User_Control_Pause:
    listener->m_action = ACTION_PAUSE;
    break;
  case CEC_User_Control_Backward:
  case CEC_User_Control_Rewind:
    listener->m_action = ACTION_SEEK_BACK_SMALL;
    break;
  case CEC_User_Control_Forward:
  case CEC_User_Control_FastForward:
    listener->m_action = ACTION_SEEK_FORWARD_SMALL;
    break;
  case CEC_User_Control_Subpicture:
  case CEC_User_Control_F2Red:
    listener->m_action = ACTION_NEXT_SUBTITLE;
    break;
  default:
    return;
  }
}

enum Action CECListener::getEvent()
{
  return (enum Action)m_action.exchange(-1);
}
