/*
 *
 *      Copyright (C) 2012 Edgar Hucek
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

extern "C" {
#include <bcm_host.h>
};
#include "OMXStreamInfo.h"
#include "OMXReader.h"
#include "utils/log.h"

#include "VideoCore.h"

using namespace std;

namespace VideoCore {

bool saved_tv_state = false;
TV_DISPLAY_STATE_T   tv_state;

void tv_stuff_init()
{
  bcm_host_init();
  atexit(bcm_host_deinit);
}

int get_mem_gpu()
{
  char response[80] = "";
  int gpu_mem = 0;
  if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
    vc_gencmd_number_property(response, "gpu", &gpu_mem);
  return gpu_mem;
}

static float get_display_aspect_ratio(HDMI_ASPECT_T aspect)
{
  switch (aspect) {
    case HDMI_ASPECT_4_3:   return 4.0/3.0;
    case HDMI_ASPECT_14_9:  return 14.0/9.0;
    case HDMI_ASPECT_16_9:  return 16.0/9.0;
    case HDMI_ASPECT_5_4:   return 5.0/4.0;
    case HDMI_ASPECT_16_10: return 16.0/10.0;
    case HDMI_ASPECT_15_9:  return 15.0/9.0;
    case HDMI_ASPECT_64_27: return 64.0/27.0;
    default:                return 16.0/9.0;
  }
}

static float get_display_aspect_ratio(SDTV_ASPECT_T aspect)
{
  switch (aspect) {
    case SDTV_ASPECT_4_3:  return 4.0/3.0;
    case SDTV_ASPECT_14_9: return 14.0/9.0;
    case SDTV_ASPECT_16_9: return 16.0/9.0;
    default:               return 4.0/3.0;
  }
}


static void CallbackTvServiceCallback(void *userdata, uint32_t reason, uint32_t param1, uint32_t param2)
{
  switch(reason)
  {
  default:
  case VC_HDMI_UNPLUGGED:
  case VC_HDMI_STANDBY:
    break;
  case VC_SDTV_NTSC:
  case VC_SDTV_PAL:
  case VC_HDMI_HDMI:
  case VC_HDMI_DVI:
    // Signal we are ready now
    sem_post((sem_t *)userdata);
    break;
  }
}

void SetVideoMode(COMXStreamInfo *hints, FORMAT_3D_T is3d, bool NativeDeinterlace)
{
  int32_t num_modes = 0;
  HDMI_RES_GROUP_T prefer_group;
  HDMI_RES_GROUP_T group = HDMI_RES_GROUP_CEA;
  float fps = 60.0f; // better to force to higher rate if no information is known
  uint32_t prefer_mode;

  if (hints->fpsrate && hints->fpsscale)
    fps = AV_TIME_BASE / OMXReader::NormalizeFrameduration((double)AV_TIME_BASE * hints->fpsscale / hints->fpsrate);

  //Supported HDMI CEA/DMT resolutions, preferred resolution will be returned
  TV_SUPPORTED_MODE_NEW_T *supported_modes = NULL;
  // query the number of modes first
  int max_supported_modes = vc_tv_hdmi_get_supported_modes_new(group, NULL, 0, &prefer_group, &prefer_mode);

  if (max_supported_modes > 0)
    supported_modes = new TV_SUPPORTED_MODE_NEW_T[max_supported_modes];

  if (supported_modes)
  {
    num_modes = vc_tv_hdmi_get_supported_modes_new(group,
        supported_modes, max_supported_modes, &prefer_group, &prefer_mode);

    CLogLog(LOGDEBUG, "EGL get supported modes (%d) = %d, prefer_group=%x, prefer_mode=%x",
        group, num_modes, prefer_group, prefer_mode);
  }

  TV_SUPPORTED_MODE_NEW_T *tv_found = NULL;

  if (num_modes > 0 && prefer_group != HDMI_RES_GROUP_INVALID)
  {
    uint32_t best_score = 1<<30;
    uint32_t scan_mode = NativeDeinterlace;

    for (int i=0; i<num_modes; i++)
    {
      TV_SUPPORTED_MODE_NEW_T *tv = supported_modes + i;
      uint32_t score = 0;
      uint32_t w = tv->width;
      uint32_t h = tv->height;
      uint32_t r = tv->frame_rate;

      /* Check if frame rate match (equal or exact multiple) */
      if(fabs(r - 1.0f*fps) / fps < 0.002f)
        score += 0;
      else if(fabs(r - 2.0f*fps) / fps < 0.002f)
        score += 1<<8;
      else
        score += (1<<16) + (1<<20)/r; // bad - but prefer higher framerate

      /* Check size too, only choose, bigger resolutions */
      if(hints->width && hints->height)
      {
        /* cost of too small a resolution is high */
        score += max((int)(hints->width - w), 0) * (1<<16);
        score += max((int)(hints->height - h), 0) * (1<<16);
        /* cost of too high a resolution is lower */
        score += max((int)(w - hints->width ), 0) * (1<<4);
        score += max((int)(h - hints->height), 0) * (1<<4);
      }

      // native is good
      if (!tv->native)
        score += 1<<16;

      // interlace is bad
      if (scan_mode != tv->scan_mode)
        score += (1<<16);

      // wanting 3D but not getting it is a negative
      if (is3d == CONF_FLAGS_FORMAT_SBS && !(tv->struct_3d_mask & HDMI_3D_STRUCT_SIDE_BY_SIDE_HALF_HORIZONTAL))
        score += 1<<18;
      if (is3d == CONF_FLAGS_FORMAT_TB  && !(tv->struct_3d_mask & HDMI_3D_STRUCT_TOP_AND_BOTTOM))
        score += 1<<18;
      if (is3d == CONF_FLAGS_FORMAT_FP  && !(tv->struct_3d_mask & HDMI_3D_STRUCT_FRAME_PACKING))
        score += 1<<18;

      // prefer square pixels modes
      float par = get_display_aspect_ratio((HDMI_ASPECT_T)tv->aspect_ratio)*(float)tv->height/(float)tv->width;
      score += fabs(par - 1.0f) * (1<<12);

      /*printf("mode %dx%d@%d %s%s:%x par=%.2f score=%d\n", tv->width, tv->height,
             tv->frame_rate, tv->native?"N":"", tv->scan_mode?"I":"", tv->code, par, score);*/

      if (score < best_score)
      {
        tv_found = tv;
        best_score = score;
      }
    }
  }

  if(tv_found)
  {
    printf("Output mode %d: %dx%d@%d %s%s:%x\n", tv_found->code, tv_found->width, tv_found->height,
           tv_found->frame_rate, tv_found->native?"N":"", tv_found->scan_mode?"I":"", tv_found->code);
    if (NativeDeinterlace && tv_found->scan_mode)
    {
      char response[80];
      vc_gencmd(response, sizeof response, "hvs_update_fields %d", 1);

      atexit(turnOffNativeDeinterlace);
    }

    // if we are closer to ntsc version of framerate, let gpu know
    int ifps = (int)(fps+0.5f);
    bool ntsc_freq = fabs(fps*1001.0f/1000.0f - ifps) < fabs(fps-ifps);

    /* inform TV of ntsc setting */
    HDMI_PROPERTY_PARAM_T property;
    property.property = HDMI_PROPERTY_PIXEL_CLOCK_TYPE;
    property.param1 = ntsc_freq ? HDMI_PIXEL_CLOCK_TYPE_NTSC : HDMI_PIXEL_CLOCK_TYPE_PAL;
    property.param2 = 0;

    /* inform TV of any 3D settings. Note this property just applies to next hdmi mode change, so no need to call for 2D modes */
    property.property = HDMI_PROPERTY_3D_STRUCTURE;
    property.param1 = HDMI_3D_FORMAT_NONE;
    property.param2 = 0;
    if (is3d != CONF_FLAGS_FORMAT_NONE)
    {
      if (is3d == CONF_FLAGS_FORMAT_SBS && tv_found->struct_3d_mask & HDMI_3D_STRUCT_SIDE_BY_SIDE_HALF_HORIZONTAL)
        property.param1 = HDMI_3D_FORMAT_SBS_HALF;
      else if (is3d == CONF_FLAGS_FORMAT_TB && tv_found->struct_3d_mask & HDMI_3D_STRUCT_TOP_AND_BOTTOM)
        property.param1 = HDMI_3D_FORMAT_TB_HALF;
      else if (is3d == CONF_FLAGS_FORMAT_FP && tv_found->struct_3d_mask & HDMI_3D_STRUCT_FRAME_PACKING)
        property.param1 = HDMI_3D_FORMAT_FRAME_PACKING;
      vc_tv_hdmi_set_property(&property);
    }

    printf("ntsc_freq:%d %s\n", ntsc_freq, property.param1 == HDMI_3D_FORMAT_SBS_HALF ? "3DSBS" :
            property.param1 == HDMI_3D_FORMAT_TB_HALF ? "3DTB" : property.param1 == HDMI_3D_FORMAT_FRAME_PACKING ? "3DFP":"");
    sem_t tv_synced;
    sem_init(&tv_synced, 0, 0);
    vc_tv_register_callback(CallbackTvServiceCallback, &tv_synced);
    int success = vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_HDMI, (HDMI_RES_GROUP_T)group, tv_found->code);
    if (success == 0)
      sem_wait(&tv_synced);
    vc_tv_unregister_callback(CallbackTvServiceCallback);
    sem_destroy(&tv_synced);
  }
  if (supported_modes)
    delete[] supported_modes;
}

bool blank_background(uint32_t rgba, int layer, int video_display)
{
  // if alpha is fully transparent then background has no effect
  if (!(rgba & 0xff000000))
    return true;

  // we create a 1x1 black pixel image that is added to display just behind video
  DISPMANX_DISPLAY_HANDLE_T   display;
  DISPMANX_UPDATE_HANDLE_T    update;
  DISPMANX_RESOURCE_HANDLE_T  resource;
  DISPMANX_ELEMENT_HANDLE_T   element;
  int             ret;
  uint32_t vc_image_ptr;
  VC_IMAGE_TYPE_T type = VC_IMAGE_ARGB8888;
  layer--;

  VC_RECT_T dst_rect, src_rect;

  display = vc_dispmanx_display_open(video_display);
  if(!display) return false;

  resource = vc_dispmanx_resource_create( type, 1 /*width*/, 1 /*height*/, &vc_image_ptr );
  if(!resource) return false;

  vc_dispmanx_rect_set( &dst_rect, 0, 0, 1, 1);

  ret = vc_dispmanx_resource_write_data( resource, type, sizeof(rgba), &rgba, &dst_rect );
  if(ret != 0) return false;

  vc_dispmanx_rect_set( &src_rect, 0, 0, 1<<16, 1<<16);
  vc_dispmanx_rect_set( &dst_rect, 0, 0, 0, 0);

  update = vc_dispmanx_update_start(0);
  if(!update) return false;

  element = vc_dispmanx_element_add(update, display, layer, &dst_rect, resource, &src_rect,
                                    DISPMANX_PROTECTION_NONE, NULL, NULL, DISPMANX_STEREOSCOPIC_MONO );
  if(!element) return false;

  ret = vc_dispmanx_update_submit_sync( update );
  if(ret != 0) return false;

  return true;
}

static void restoreTVState()
{
  if(saved_tv_state && tv_state.display.hdmi.group && tv_state.display.hdmi.mode)
    vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_HDMI, (HDMI_RES_GROUP_T)tv_state.display.hdmi.group, tv_state.display.hdmi.mode);
}

void saveTVState()
{
  if(!saved_tv_state)
  {
    memset(&tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
    vc_tv_get_display_state(&tv_state);
    saved_tv_state = true;
    atexit(restoreTVState);
  }
}

float getDisplayAspect()
{
  float display_aspect;

  TV_DISPLAY_STATE_T current_tv_state;
  memset(&current_tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
  vc_tv_get_display_state(&current_tv_state);
  if(current_tv_state.state & ( VC_HDMI_HDMI | VC_HDMI_DVI )) {
    //HDMI or DVI on
    display_aspect = get_display_aspect_ratio((HDMI_ASPECT_T)current_tv_state.display.hdmi.aspect_ratio);
  } else {
    //composite on
    display_aspect = get_display_aspect_ratio((SDTV_ASPECT_T)current_tv_state.display.sdtv.display_options.aspect);
  }
  display_aspect *= (float)current_tv_state.display.hdmi.height/(float)current_tv_state.display.hdmi.width;

  return display_aspect;
}

const char *getAudioDevice()
{
  if (vc_tv_hdmi_audio_supported(EDID_AudioFormat_ePCM, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) == 0)
    return "omx:hdmi";
  else
    return "omx:local";
}

bool canPassThroughAC3()
{
  return vc_tv_hdmi_audio_supported(EDID_AudioFormat_eAC3, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit) != 0;
}

bool canPassThroughDTS()
{
  return vc_tv_hdmi_audio_supported(EDID_AudioFormat_eDTS, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit) != 0;
}

void turnOffNativeDeinterlace()
{
    char response[80];
    vc_gencmd(response, sizeof response, "hvs_update_fields %d", 0);
}

} // namespace
