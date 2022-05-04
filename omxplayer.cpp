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

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>

extern "C" {
#include <bcm_host.h>
};

#include "OMXStreamInfo.h"

#include "utils/log.h"

#include "DllAvUtil.h"
#include "DllAvCodec.h"

#include "OMXVideo.h"
#include "utils/PCMRemap.h"
#include "OMXClock.h"
#include "OMXAudio.h"
#include "OMXReader.h"
#include "OMXPlayerVideo.h"
#include "OMXPlayerAudio.h"
#include "OMXPlayerSubtitles.h"
#include "OMXDvdPlayer.h"
#include "OMXControl.h"
#include "DllOMX.h"
#include "KeyConfig.h"
#include "Keyboard.h"
#include "utils/RegExp.h"
#include "AutoPlaylist.h"
#include "RecentFileStore.h"
#include "RecentDVDStore.h"
#include "utils/uri_unescape.h"

#include <string>
#include <utility>

#include "version.h"

typedef enum {CONF_FLAGS_FORMAT_NONE, CONF_FLAGS_FORMAT_SBS, CONF_FLAGS_FORMAT_TB, CONF_FLAGS_FORMAT_FP } FORMAT_3D_T;
enum PCMChannels  *m_pChannelMap        = NULL;
volatile sig_atomic_t m_stopped           = false;
long              m_Volume              = 0;
long              m_Amplification       = 0;
bool              m_NativeDeinterlace   = false;
bool              m_HWDecode            = false;
bool              m_osd                 = true;
bool              m_keys                = true;
std::string       m_external_subtitles_path;
bool              m_has_external_subtitles = false;
std::string       m_dbus_name           = "org.mpris.MediaPlayer2.omxplayer";
bool              m_Pause               = false;
OMXReader         m_omx_reader;
int               m_audio_index     = -1;
OMXClock          *m_av_clock           = NULL;
OMXControl        m_omxcontrol;
Keyboard          m_keyboard;
OMXAudioConfig    m_config_audio;
OMXVideoConfig    m_config_video;
OMXPacket         *m_omx_pkt            = NULL;
int               m_subtitle_index      = -1;
OMXPlayerVideo    m_player_video;
OMXPlayerAudio    m_player_audio;
OMXPlayerSubtitles  m_player_subtitles;
bool              m_has_video           = false;
bool              m_has_audio           = false;
bool              m_has_subtitle        = false;
bool              m_loop                = false;
RecentFileStore   m_file_store;
RecentDVDStore    m_dvd_store;
AutoPlaylist      m_playlist;
bool              m_firstfile           = true;
bool              m_send_eos            = false;
bool              m_seek_flush          = false;
std::string       m_filename;
int               m_track               = -1;
bool              m_is_dvd              = false;
bool              m_is_dvd_device       = false;
OMXDvdPlayer      *m_DvdPlayer          = NULL;
int               m_incr                = -1;
int               m_loop_from           = 0;
COMXCore          g_OMX;
bool              m_stats               = false;
bool              m_dump_format         = false;
bool              m_dump_format_exit    = false;
FORMAT_3D_T       m_3d                  = CONF_FLAGS_FORMAT_NONE;
bool              m_refresh             = false;
float             m_threshold           = -1.0f; // amount of audio/video required to come out of buffering
float             m_timeout             = 10.0f; // amount of time file/network operation can stall for before timing out
int               m_orientation         = -1; // unset
float             m_fps                 = 0.0f; // unset
TV_DISPLAY_STATE_T   tv_state;
std::string       m_cookie;
std::string       m_user_agent;
std::string       m_lavfdopts;
std::string       m_avdict;
int               m_next_prev_file      = 0;
char              m_audio_lang[4]       = "\0";
char              m_subtitle_lang[4]    = "\0";
std::string       m_replacement_filename;
bool              m_playlist_enabled    = true;
float             m_latency             = 0.0f;
int               m_control_err;
bool              m_ext_subs_showing    = false;

#define S(x) (int)(DVD_PLAYSPEED_NORMAL*(x))
int playspeeds[] = {S(0), S(1/16.0), S(1/8.0), S(1/4.0), S(1/2.0), S(0.975), S(1.0), S(1.125), S(2.0), S(4.0)};
const int playspeed_max = 9, playspeed_normal = 6;
int playspeed_current = playspeed_normal;

enum{ERROR=-1,SUCCESS,ONEBYTE};

enum {
//EXIT_SUCCESS = 0,
//EXIT_FAILURE = 2,
  PLAY_STOPPED = 3,
  CHANGE_FILE = 4,
  CHANGE_PLAYLIST_ITEM = 5,
  RUN_PLAY_LOOP = 6,
  END_PLAY = 7,
  END_PLAY_WITH_ERROR = 8,
  ABORT_PLAY = 9,
  SHUTDOWN = 10,
};


// SIGUSR1 is an error in a thread so exit
// otherwise set m_stopped for an orderly winddown
void sig_handler(int s)
{
  if(s == SIGUSR1)
    exit(1);
  else
    m_stopped = true;
}

void print_usage()
{
  printf(
#include "help.h"
  );
}

void print_keybindings()
{
  printf(
#include "keys.h"
  );
}

void print_version()
{
  printf("omxplayer - Commandline multimedia player for the Raspberry Pi\n");
  printf("        Build date: %s\n", VERSION_DATE);
  printf("        Version   : %s [%s]\n", VERSION_HASH, VERSION_BRANCH);
  printf("        Repository: %s\n", VERSION_REPO);
}

enum {
    UM_STDOUT = 0b00000001,
    UM_LONG   = 0b00000010,
    UM_SLEEP  = 0b00000100,
    UM_NORM   = 0b00000000,
    UM_ALL    = 0b00000111,
};

void osd_print(int options, const char *msg)
{
  if(m_osd)
    m_player_subtitles.DisplayText(msg, options & UM_LONG ? 3000 : 1500);

  if(options & UM_STDOUT)
  {
    char *s = strdup(msg);
    if(s) {
      char *p = s;
      while(*p != '\0') {
        if(*p == '\n') *p = ' ';
        p++;
      }

      puts(s);
      free(s);
    }
  }

  // useful when we want to display some osd before exiting the program
  if(m_osd && (options & UM_SLEEP)) m_av_clock->OMXSleep(options & UM_LONG ? 3000 : 1500);
}

void osd_print(const char *msg)
{
  osd_print(UM_NORM, msg);
}

#ifdef __GNUC__
void osd_printf(int options, const char* format, ...) __attribute__((format(printf,2,3)));
#endif

void osd_printf(int options, const char* format, ...)
{
    char buffer[60];
    va_list va;
    va_start(va, format);
    vsnprintf(buffer, 60, format, va);
    va_end(va);

    osd_print(options, &buffer[0]);
}

int exit_with_message(const char *msg)
{
  //osd_print(UM_ALL, msg);
  puts(msg);
  return END_PLAY_WITH_ERROR;
}

void show_progress_message(const char *msg, int pos, int dur, int options = UM_NORM)
{
  osd_printf(options, "%s\n%02d:%02d:%02d / %02d:%02d:%02d",
                msg, (pos/3600), (pos/60)%60, pos%60, (dur/3600), (dur/60)%60, dur%60);
}

static void UpdateRaspicastMetaData(const string &msg)
{
  FILE *fp = fopen("/dev/shm/.r_info", "w");
  if(fp == NULL) return;

  fputs("local\n", fp);
  fputs(msg.c_str(), fp);
  fputs("\n", fp);
  fclose(fp);
}

static void PrintSubtitleInfo()
{
  int count = 0;
  int index = 0;

  if(m_has_external_subtitles) {
    count = 1;
  } else if(m_has_subtitle) {
    count = m_omx_reader.SubtitleStreamCount();
    index = m_player_subtitles.GetActiveStream();
  }

  printf("Subtitle count: %d, state: %s, index: %u, delay: %d\n",
         count,
         m_has_subtitle && m_player_subtitles.GetVisible() ? " on" : "off",
         index+1,
         m_has_subtitle ? m_player_subtitles.GetDelay() : 0);
}

static void FlushStreams(int64_t pts);

static void SetSpeed(int iSpeed)
{
  if(!m_av_clock)
    return;

  m_omx_reader.SetSpeed(iSpeed);

  m_av_clock->OMXSetSpeed(iSpeed);
  m_av_clock->OMXSetSpeed(iSpeed, true, true);
}

static float get_display_aspect_ratio(HDMI_ASPECT_T aspect)
{
  float display_aspect;
  switch (aspect) {
    case HDMI_ASPECT_4_3:   display_aspect = 4.0/3.0;   break;
    case HDMI_ASPECT_14_9:  display_aspect = 14.0/9.0;  break;
    case HDMI_ASPECT_16_9:  display_aspect = 16.0/9.0;  break;
    case HDMI_ASPECT_5_4:   display_aspect = 5.0/4.0;   break;
    case HDMI_ASPECT_16_10: display_aspect = 16.0/10.0; break;
    case HDMI_ASPECT_15_9:  display_aspect = 15.0/9.0;  break;
    case HDMI_ASPECT_64_27: display_aspect = 64.0/27.0; break;
    default:                display_aspect = 16.0/9.0;  break;
  }
  return display_aspect;
}

static float get_display_aspect_ratio(SDTV_ASPECT_T aspect)
{
  float display_aspect;
  switch (aspect) {
    case SDTV_ASPECT_4_3:  display_aspect = 4.0/3.0;  break;
    case SDTV_ASPECT_14_9: display_aspect = 14.0/9.0; break;
    case SDTV_ASPECT_16_9: display_aspect = 16.0/9.0; break;
    default:               display_aspect = 4.0/3.0;  break;
  }
  return display_aspect;
}

static void FlushStreams(int64_t pts)
{
  m_av_clock->OMXStop();
  m_av_clock->OMXPause();

  if(m_has_video)
    m_player_video.Flush();

  if(m_has_audio)
    m_player_audio.Flush();

  if(pts != AV_NOPTS_VALUE)
    m_av_clock->OMXMediaTime(pts);

  if(m_has_subtitle)
    m_player_subtitles.Flush();

  if(m_omx_pkt)
  {
    delete m_omx_pkt;
    m_omx_pkt = NULL;
  }
}

static void CallbackTvServiceCallback(void *userdata, uint32_t reason, uint32_t param1, uint32_t param2)
{
  sem_t *tv_synced = (sem_t *)userdata;
  switch(reason)
  {
  case VC_HDMI_UNPLUGGED:
    break;
  case VC_HDMI_STANDBY:
    break;
  case VC_SDTV_NTSC:
  case VC_SDTV_PAL:
  case VC_HDMI_HDMI:
  case VC_HDMI_DVI:
    // Signal we are ready now
    sem_post(tv_synced);
    break;
  default:
    break;
  }
}

void SetVideoMode(int width, int height, int fpsrate, int fpsscale, FORMAT_3D_T is3d)
{
  int32_t num_modes = 0;
  int i;
  HDMI_RES_GROUP_T prefer_group;
  HDMI_RES_GROUP_T group = HDMI_RES_GROUP_CEA;
  float fps = 60.0f; // better to force to higher rate if no information is known
  uint32_t prefer_mode;

  if (fpsrate && fpsscale)
    fps = AV_TIME_BASE / OMXReader::NormalizeFrameduration((double)AV_TIME_BASE * fpsscale / fpsrate);

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
    uint32_t scan_mode = m_NativeDeinterlace;

    for (i=0; i<num_modes; i++)
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
      if(width && height) 
      {
        /* cost of too small a resolution is high */
        score += max((int)(width -w), 0) * (1<<16);
        score += max((int)(height-h), 0) * (1<<16);
        /* cost of too high a resolution is lower */
        score += max((int)(w-width ), 0) * (1<<4);
        score += max((int)(h-height), 0) * (1<<4);
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
    char response[80];
    printf("Output mode %d: %dx%d@%d %s%s:%x\n", tv_found->code, tv_found->width, tv_found->height, 
           tv_found->frame_rate, tv_found->native?"N":"", tv_found->scan_mode?"I":"", tv_found->code);
    if (m_NativeDeinterlace && tv_found->scan_mode)
      vc_gencmd(response, sizeof response, "hvs_update_fields %d", 1);

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

// Check file exists and is readable
bool Exists(const std::string& path)
{
  FILE *file;
  if((file = fopen(path.c_str(), "r")))
  {
    fclose(file);
    return true;
  }
  return false;
}

bool IsURL(const std::string &str)
{
  static CRegExp protocol_match("^[a-zA-Z]+://");
  return protocol_match.RegFind(str, 0) > -1;
}

bool IsPipe(const std::string& str)
{
  return str.substr(0, 5) == "pipe:";
}

static int get_mem_gpu(void)
{
  char response[80] = "";
  int gpu_mem = 0;
  if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
    vc_gencmd_number_property(response, "gpu", &gpu_mem);
  return gpu_mem;
}

static bool blank_background(uint32_t rgba)
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
  int             layer = m_config_video.layer - 1;

  VC_RECT_T dst_rect, src_rect;

  display = vc_dispmanx_display_open(m_config_video.display);
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

int ExitFileNotFound(const std::string& path)
{
  osd_printf(UM_ALL, "File \"%s\" not found.", path.c_str());

  if (m_av_clock)
    delete m_av_clock;

  bcm_host_deinit();
  g_OMX.Deinitialize();
  return EXIT_FAILURE;
}

void ChangeSubtitle(int delta)
{
  m_player_subtitles.SetActiveStreamDelta(delta);
  if(!m_player_subtitles.GetVisible()) {
    m_subtitle_lang[0] = '\0';
    osd_print("Subtitles Off");
  } else {
    int new_index = m_player_subtitles.GetActiveStream();
    strcpy(m_subtitle_lang, m_omx_reader.GetStreamLanguage(OMXSTREAM_SUBTITLE,
        new_index).c_str());
    if(m_subtitle_lang[0] != '\0')
        osd_printf(UM_NORM, "Subtitle stream: %s", m_subtitle_lang);
    else
        osd_printf(UM_NORM, "Subtitle stream: %d", new_index + 1);
  }
  PrintSubtitleInfo();
}

void PlayPause(bool new_status)
{
  m_Pause = new_status;

  if (m_av_clock->OMXPlaySpeed() != DVD_PLAYSPEED_NORMAL &&
      m_av_clock->OMXPlaySpeed() != DVD_PLAYSPEED_PAUSE)
  {
    puts("resume");
    playspeed_current = playspeed_normal;
    SetSpeed(playspeeds[playspeed_current]);
    m_seek_flush = true;
  }

  if(m_has_subtitle)
  {
    if(m_Pause) m_player_subtitles.Pause();
    else m_player_subtitles.Resume();
  }

  int t = m_av_clock->OMXMediaTime() * 1e-6;
  int dur = m_omx_reader.GetStreamLengthSeconds();
  show_progress_message(m_Pause ? "Pause" : "Play", t, dur);
}

int change_file();
int change_playlist_item();
int run_play_loop();
void end_of_play_loop();
int playlist_control();
int shutdown(bool exit_with_error = false);

int main(int argc, char *argv[])
{
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGUSR1, sig_handler);

  const int font_size_opt   = 0x101;
  const int align_opt       = 0x102;
  const int no_ghost_box_opt = 0x203;
  const int subtitles_opt   = 0x103;
  const int lines_opt       = 0x104;
  const int vol_opt         = 0x106;
  const int audio_fifo_opt  = 0x107;
  const int video_fifo_opt  = 0x108;
  const int audio_queue_opt = 0x109;
  const int video_queue_opt = 0x10a;
  const int no_deinterlace_opt = 0x10b;
  const int threshold_opt   = 0x10c;
  const int timeout_opt     = 0x10f;
  const int boost_on_downmix_opt = 0x200;
  const int no_boost_on_downmix_opt = 0x207;
  const int key_config_opt  = 0x10d;
  const int amp_opt         = 0x10e;
  const int no_osd_opt      = 0x202;
  const int orientation_opt = 0x204;
  const int fps_opt         = 0x208;
  const int live_opt        = 0x205;
  const int layout_opt      = 0x206;
  const int dbus_name_opt   = 0x209;
  const int loop_opt        = 0x20a;
  const int layer_opt       = 0x20b;
  const int no_keys_opt     = 0x20c;
  const int anaglyph_opt    = 0x20d;
  const int native_deinterlace_opt = 0x20e;
  const int display_opt     = 0x20f;
  const int alpha_opt       = 0x210;
  const int advanced_opt    = 0x211;
  const int aspect_mode_opt = 0x212;
  const int http_cookie_opt = 0x300;
  const int http_user_agent_opt = 0x301;
  const int lavfdopts_opt   = 0x400;
  const int avdict_opt      = 0x401;
  const int track_opt       = 0x402;
  const int start_paused_opt = 0x403;

  struct option longopts[] = {
    { "info",         no_argument,        NULL,          'i' },
    { "with-info",    no_argument,        NULL,          'I' },
    { "help",         no_argument,        NULL,          'h' },
    { "version",      no_argument,        NULL,          'v' },
    { "keys",         no_argument,        NULL,          'k' },
    { "aidx",         required_argument,  NULL,          'n' },
    { "adev",         required_argument,  NULL,          'o' },
    { "stats",        no_argument,        NULL,          's' },
    { "passthrough",  no_argument,        NULL,          'p' },
    { "vol",          required_argument,  NULL,          vol_opt },
    { "amp",          required_argument,  NULL,          amp_opt },
    { "deinterlace",  no_argument,        NULL,          'd' },
    { "nodeinterlace",no_argument,        NULL,          no_deinterlace_opt },
    { "nativedeinterlace",no_argument,    NULL,          native_deinterlace_opt },
    { "anaglyph",     required_argument,  NULL,          anaglyph_opt },
    { "advanced",     optional_argument,  NULL,          advanced_opt },
    { "hw",           no_argument,        NULL,          'w' },
    { "3d",           required_argument,  NULL,          '3' },
    { "allow-mvc",    no_argument,        NULL,          'M' },
    { "hdmiclocksync", no_argument,       NULL,          'y' },
    { "nohdmiclocksync", no_argument,     NULL,          'z' },
    { "refresh",      no_argument,        NULL,          'r' },
    { "genlog",       optional_argument,  NULL,          'g' },
    { "sid",          required_argument,  NULL,          't' },
    { "pos",          required_argument,  NULL,          'l' },    
    { "blank",        optional_argument,  NULL,          'b' },
    { "no-playlist",  no_argument,        NULL,          'a' },
    { "font-size",    required_argument,  NULL,          font_size_opt },
    { "align",        required_argument,  NULL,          align_opt },
    { "no-ghost-box", no_argument,        NULL,          no_ghost_box_opt },
    { "subtitles",    required_argument,  NULL,          subtitles_opt },
    { "lines",        required_argument,  NULL,          lines_opt },
    { "aspect-mode",  required_argument,  NULL,          aspect_mode_opt },
    { "audio_fifo",   required_argument,  NULL,          audio_fifo_opt },
    { "video_fifo",   required_argument,  NULL,          video_fifo_opt },
    { "audio_queue",  required_argument,  NULL,          audio_queue_opt },
    { "video_queue",  required_argument,  NULL,          video_queue_opt },
    { "threshold",    required_argument,  NULL,          threshold_opt },
    { "timeout",      required_argument,  NULL,          timeout_opt },
    { "boost-on-downmix", no_argument,    NULL,          boost_on_downmix_opt },
    { "no-boost-on-downmix", no_argument, NULL,          no_boost_on_downmix_opt },
    { "key-config",   required_argument,  NULL,          key_config_opt },
    { "no-osd",       no_argument,        NULL,          no_osd_opt },
    { "no-keys",      no_argument,        NULL,          no_keys_opt },
    { "orientation",  required_argument,  NULL,          orientation_opt },
    { "fps",          required_argument,  NULL,          fps_opt },
    { "live",         no_argument,        NULL,          live_opt },
    { "layout",       required_argument,  NULL,          layout_opt },
    { "dbus_name",    required_argument,  NULL,          dbus_name_opt },
    { "loop",         no_argument,        NULL,          loop_opt },
    { "layer",        required_argument,  NULL,          layer_opt },
    { "alpha",        required_argument,  NULL,          alpha_opt },
    { "display",      required_argument,  NULL,          display_opt },
    { "cookie",       required_argument,  NULL,          http_cookie_opt },
    { "user-agent",   required_argument,  NULL,          http_user_agent_opt },
    { "lavfdopts",    required_argument,  NULL,          lavfdopts_opt },
    { "avdict",       required_argument,  NULL,          avdict_opt },
    { "track",        required_argument,  NULL,          track_opt },
    { "start-paused", no_argument,        NULL,          start_paused_opt },
    { 0, 0, 0, 0 }
  };

  int               c;
  std::string       mode;
  bool              centered            = false;
  bool              ghost_box           = true;
  unsigned int      subtitle_lines      = 3;
  float             font_size           = 0.055f;
  bool              no_hdmi_clock_sync  = false;
  uint32_t          background  = 0;
  std::string       keymap_file;

  while ((c = getopt_long(argc, argv, "awiIhvkn:l:o:cslb::pd3:Myzt:rg", longopts, NULL)) != -1)
  {
    switch (c) 
    {
      case 'r':
        m_refresh = true;
        break;
      case 'g':
        CLogInit(LOGDEBUG, optarg ? optarg : "./omxplayer.log");
        break;
      case 'y':
        m_config_video.hdmi_clock_sync = true;
        break;
      case 'z':
        no_hdmi_clock_sync = true;
        break;
      case '3':
        mode = optarg;
        if(mode == "TB")
          m_3d = CONF_FLAGS_FORMAT_TB;
        else if(mode == "FP")
          m_3d = CONF_FLAGS_FORMAT_FP;
        else if(mode == "SBS")
          m_3d = CONF_FLAGS_FORMAT_SBS;
        else
        {
          print_usage();
          return EXIT_FAILURE;
        }
        m_config_video.allow_mvc = true;
        break;
      case 'M':
        m_config_video.allow_mvc = true;
        break;
      case 'd':
        m_config_video.deinterlace = VS_DEINTERLACEMODE_FORCE;
        break;
      case no_deinterlace_opt:
        m_config_video.deinterlace = VS_DEINTERLACEMODE_OFF;
        break;
      case native_deinterlace_opt:
        m_config_video.deinterlace = VS_DEINTERLACEMODE_OFF;
        m_NativeDeinterlace = true;
        break;
      case anaglyph_opt:
        m_config_video.anaglyph = (OMX_IMAGEFILTERANAGLYPHTYPE)atoi(optarg);
        break;
      case advanced_opt:
        m_config_video.advanced_hd_deinterlace = optarg ? (atoi(optarg) ? true : false): true;
        break;
      case 'w':
        m_config_audio.hwdecode = true;
        break;
      case 'p':
        m_config_audio.passthrough = true;
        break;
      case 's':
        m_stats = true;
        break;
      case 'o':
        {
          std::string str = optarg;
          int colon = str.find(':');
          if(colon >= 0)
          {
            m_config_audio.device = str.substr(0, colon);
            m_config_audio.subdevice = str.substr(colon + 1, str.length() - colon);
          }
          else
          {
            m_config_audio.device = str;
            m_config_audio.subdevice.clear();
          }
        }
        if(m_config_audio.device != "local" && m_config_audio.device != "hdmi" && m_config_audio.device != "both" &&
           m_config_audio.device != "alsa")
        {
          printf("Bad argument for -%c: Output device must be `local', `hdmi', `both' or `alsa'\n", c);
          return EXIT_FAILURE;
        }
        m_config_audio.device = "omx:" + m_config_audio.device;
        break;
      case 'i':
        m_dump_format      = true;
        m_dump_format_exit = true;
        m_osd              = false;
        break;
      case 'I':
        m_dump_format = true;
        break;
      case 't':
        if(optarg[0] >= '0' && optarg[0] <= '9')
        {
          m_subtitle_index = atoi(optarg) - 1;
          if(m_subtitle_index < 0)
            m_subtitle_index = 0;
        }
        else
        {
          strncpy(m_subtitle_lang, optarg, 3);
          m_subtitle_lang[3] = '\0';
        }
        break;
      case 'n':
        if(optarg[0] >= '0' && optarg[0] <= '9')
        {
          m_audio_index = atoi(optarg) - 1;
        }
        else
        {
          strncpy(m_audio_lang, optarg, 3);
          m_audio_lang[3] = '\0';
        }
        break;
      case 'l':
        {
          if(strchr(optarg, ':'))
          {
            unsigned int h, m, s;
            if(sscanf(optarg, "%u:%u:%u", &h, &m, &s) == 3)
              m_incr = h*3600 + m*60 + s;
          }
          else
          {
            m_incr = atoi(optarg);
          }
          if(m_loop)
            m_loop_from = m_incr;
        }
        break;
      case 'a':
        m_playlist_enabled = false;
        break;
      case no_osd_opt:
        m_osd = false;
        break;
      case no_keys_opt:
        m_keys = false;
        break;
      case font_size_opt:
        {
          const int thousands = atoi(optarg);
          if (thousands > 0)
            font_size = thousands*0.001f;
        }
        break;
      case align_opt:
        centered = !strcmp(optarg, "center");
        break;
      case no_ghost_box_opt:
        ghost_box = false;
        break;
      case subtitles_opt:
        m_external_subtitles_path = optarg;
        m_has_external_subtitles = true;
        m_subtitle_index = 0;
        break;
      case lines_opt:
        subtitle_lines = std::max(atoi(optarg), 1);
        break;
      case aspect_mode_opt:
        if (optarg) {
          if (!strcasecmp(optarg, "letterbox"))
            m_config_video.aspectMode = 1;
          else if (!strcasecmp(optarg, "fill"))
            m_config_video.aspectMode = 2;
          else if (!strcasecmp(optarg, "stretch"))
            m_config_video.aspectMode = 3;
          else
            m_config_video.aspectMode = 0;
        }
        break;
      case vol_opt:
        m_Volume = atoi(optarg);
        break;
      case amp_opt:
        m_Amplification = atoi(optarg);
        break;
      case boost_on_downmix_opt:
        m_config_audio.boostOnDownmix = true;
        break;
      case no_boost_on_downmix_opt:
        m_config_audio.boostOnDownmix = false;
        break;
      case audio_fifo_opt:
        m_config_audio.fifo_size = atof(optarg);
        break;
      case video_fifo_opt:
        m_config_video.fifo_size = atof(optarg);
        break;
      case audio_queue_opt:
        m_config_audio.queue_size = atof(optarg);
        break;
      case video_queue_opt:
        m_config_video.queue_size = atof(optarg);
        break;
      case threshold_opt:
        m_threshold = atof(optarg);
        break;
      case timeout_opt:
        m_timeout = atof(optarg);
        break;
      case orientation_opt:
        m_orientation = atoi(optarg);
        break;
      case fps_opt:
        m_fps = atof(optarg);
        break;
      case live_opt:
        m_config_audio.is_live = true;
        break;
      case layout_opt:
      {
        const char *layouts[] = {"2.0", "2.1", "3.0", "3.1", "4.0", "4.1", "5.0", "5.1", "7.0", "7.1"};
        unsigned i;
        for (i=0; i<sizeof layouts/sizeof *layouts; i++)
          if (strcmp(optarg, layouts[i]) == 0)
          {
            m_config_audio.layout = (enum PCMLayout)i;
            break;
          }
        if (i == sizeof layouts/sizeof *layouts)
        {
          printf("Wrong layout specified: %s\n", optarg);
          print_usage();
          return EXIT_FAILURE;
        }
        break;
      }
      case dbus_name_opt:
        m_dbus_name = optarg;
        break;
      case loop_opt:
        if(m_incr > 0)
            m_loop_from = m_incr;
        m_loop = true;
        m_playlist_enabled = false;
        break;
      case 'b':
        background = optarg ? strtoul(optarg, NULL, 0) : 0xff000000;
        break;
      case key_config_opt:
        keymap_file = optarg;
        break;
      case layer_opt:
        m_config_video.layer = atoi(optarg);
        break;
      case alpha_opt:
        m_config_video.alpha = atoi(optarg);
        break;
      case display_opt:
        m_config_video.display = atoi(optarg);
        break;
      case http_cookie_opt:
        m_cookie = optarg;
        break;
      case http_user_agent_opt:
        m_user_agent = optarg;
        break;    
      case lavfdopts_opt:
        m_lavfdopts = optarg;
        break;
      case avdict_opt:
        m_avdict = optarg;
        break;
      case track_opt:
        m_track = atoi(optarg) - 1;
        if(m_track < 0) m_track = -1;
      case start_paused_opt:
        m_Pause = true;
        break;
      case 0:
        break;
      case 'h':
        print_usage();
        return EXIT_SUCCESS;
        break;
      case 'v':
        print_version();
        return EXIT_SUCCESS;
        break;
      case 'k':
        print_keybindings();
        return EXIT_SUCCESS;
        break;
      case ':':
        return EXIT_FAILURE;
        break;
      default:
        return EXIT_FAILURE;
        break;
    }
  }

  if (optind >= argc) {
    print_usage();
    return EXIT_SUCCESS;
  }

  bcm_host_init();
  if(!g_OMX.Initialize())
  {
    bcm_host_deinit();
    return EXIT_FAILURE;
  }

  // start the clock
  m_av_clock = new OMXClock();
  if(!m_av_clock->OMXInitialize())
  {
    bcm_host_deinit();
    g_OMX.Deinitialize();
    return EXIT_FAILURE;
  }

  if(!blank_background(background))
  {
    if (m_av_clock)
      delete m_av_clock;
    bcm_host_deinit();
    g_OMX.Deinitialize();
    return EXIT_FAILURE;
  }

  // init subtitle object
  if(!m_player_subtitles.Init(m_config_video.display,
                                m_config_video.layer,
                                font_size,
                                centered,
                                ghost_box,
                                subtitle_lines,
                                m_av_clock))
  {
    if (m_av_clock)
      delete m_av_clock;
    bcm_host_deinit();
    g_OMX.Deinitialize();
    return EXIT_FAILURE;
  }

  osd_print("Loading...");

  int gpu_mem = get_mem_gpu();
  int min_gpu_mem = 64;
  if (gpu_mem > 0 && gpu_mem < min_gpu_mem)
    printf("Only %dM of gpu_mem is configured. Try running \"sudo raspi-config\" and ensure that \"memory_split\" has a value of %d or greater\n", gpu_mem, min_gpu_mem);

  m_control_err = m_omxcontrol.init(
    m_av_clock,
    &m_player_audio,
    &m_player_subtitles,
    &m_omx_reader,
    m_dbus_name
  );

  // 3d modes don't work without switch hdmi mode
  if (m_3d != CONF_FLAGS_FORMAT_NONE || m_NativeDeinterlace)
    m_refresh = true;

  // you really don't want want to match refresh rate without hdmi clock sync
  if ((m_refresh || m_NativeDeinterlace) && !no_hdmi_clock_sync)
    m_config_video.hdmi_clock_sync = true;

  if(m_config_video.hdmi_clock_sync && !m_av_clock->HDMIClockSync())
  {
    if (m_av_clock)
      delete m_av_clock;
    bcm_host_deinit();
    g_OMX.Deinitialize();
    return EXIT_FAILURE;
  }

  // get filename
  m_filename = argv[optind];

  // disable keys when using stdin for input
  if(m_filename == "pipe:" || m_filename == "pipe:0")
    m_keys = false;

  if(m_keys)
    m_keyboard.Init(keymap_file, m_dbus_name);

  // Disable seeking and playlists when reading from a pipe
  if(IsPipe(m_filename))
  {
    m_config_audio.is_live = true;
    m_playlist_enabled = false;
  }

  // check if command line provided subtitles file exists
  if(m_has_external_subtitles && !Exists(m_external_subtitles_path))
    return ExitFileNotFound(m_external_subtitles_path);

  // control loop
  int rv = CHANGE_FILE;

  while(1) {
    switch(rv) {
    case CHANGE_FILE:
      rv = change_file();
      break;
    case CHANGE_PLAYLIST_ITEM:
      rv = change_playlist_item();
      break;
    case RUN_PLAY_LOOP:
      rv = run_play_loop();
      end_of_play_loop();
      break;
    case END_PLAY_WITH_ERROR:
      rv = shutdown(true);
      break;
    case ABORT_PLAY:
      m_stopped = true;
      rv = playlist_control();
      break;
    case END_PLAY:
      rv = playlist_control();
      break;
    case SHUTDOWN:
      rv = shutdown(false);
      // fall through
    case EXIT_SUCCESS:
    case EXIT_FAILURE:
    case PLAY_STOPPED:
    default:
      return rv;
    }
  }
}

// we jump here when is provided with a new file
int change_file()
{
  // strip off file://
  if(m_filename.substr(0, 7) == "file://" )
    m_filename.erase(0, 7);

  bool is_local_file = !IsURL(m_filename) && !IsPipe(m_filename);

  bool started_from_link = false;
  if(is_local_file)
  {
    if(!Exists(m_filename))
      return ExitFileNotFound(m_filename);

    // get realpath for file
    char *fp = realpath(m_filename.c_str(), NULL);
    assert(fp != NULL);
    m_filename = fp;
    free(fp);

    // check if this is a link file
    // if it's a link file, rerun some file checks
    if(m_file_store.checkIfLink(m_filename))
    {
      started_from_link = true;
      m_file_store.readlink(m_filename, m_track, m_incr, &m_audio_lang[0], &m_subtitle_lang[0],
        m_subtitle_index);
      
      is_local_file = !IsURL(m_filename) && !IsPipe(m_filename);

      if(is_local_file && !Exists(m_filename))
        return ExitFileNotFound(m_filename);
    }
  }

  // m_filename may have changed
  m_is_dvd = false;
  m_is_dvd_device = false;
  if(is_local_file)
  {
    // Are we dealing with a DVD VIDEO_TS folder or a device file
    CRegExp findvideots("^(.*?/VIDEO_TS|/dev/.*$)");
    if(findvideots.RegFind(m_filename, 0) > -1)
    {
      m_is_dvd = true;
      m_is_dvd_device = true;
      m_filename = findvideots.GetMatch(1);
    }
    else if(!m_dump_format_exit)
    {
      // make a playlist
      m_playlist.readPlaylist(m_filename);
    }
  }

  // read the relevant recent files/dvd store
  if(!m_dump_format_exit && m_playlist_enabled)
  {
    if(m_is_dvd_device) {
      m_dvd_store.readStore();
    } else {
      m_playlist_enabled = m_file_store.readStore();

      if(!started_from_link)
        m_file_store.retrieveRecentInfo(m_filename, m_track, m_incr, &m_audio_lang[0], &m_subtitle_lang[0],
          m_subtitle_index);
    }
  }

  return CHANGE_PLAYLIST_ITEM;
}


// we jump here when playing the next item in an auto generated playlist
int change_playlist_item()
{
  // a playlist item is a new file that could be an iso or a dmg files
  if(m_filename.substr(m_filename.size()-4, 4) == ".iso"
      || m_filename.substr(m_filename.size()-4, 4) == ".dmg")
    m_is_dvd = true;

  if(m_is_dvd)
  {
    m_has_external_subtitles = false;
    try {
        m_DvdPlayer = new OMXDvdPlayer(m_filename);
    }
    catch(const char *msg) {
        return exit_with_message(msg);
    }

    m_DvdPlayer->enableHeuristicTrackSelection();

    // Was DVD played before?
    if(!m_dump_format_exit && m_is_dvd_device && m_playlist_enabled)
    {
      // check we have a valid dvd id
      if(m_DvdPlayer->GetID().empty())
        m_playlist_enabled = false;
      else
        m_dvd_store.setCurrentDVD(m_DvdPlayer->GetID(), m_track, m_incr, &m_audio_lang[0], &m_subtitle_lang[0]);
    }

    // If m_track is set to -1, look for the first enabled track
    if(m_track == -1)
      m_track = 0;

    if(!m_DvdPlayer->OpenTrack(m_track))
      return exit_with_message("Failed to open DVD track");
  }
  else
  {
    m_track = -1;

    if(m_osd && !m_has_external_subtitles && !IsURL(m_filename))
    {
      std::string subtitles_path = m_filename.substr(0, m_filename.find_last_of(".")) + ".srt";

      if(Exists(subtitles_path))
      {
        m_external_subtitles_path = subtitles_path;
        m_has_external_subtitles = true;
      }
    }
  }

  // Start from beginning
  if(m_incr == -1)
    m_incr = 0;

  return RUN_PLAY_LOOP;
}

// we jump here when playing the next track in a dvd
int run_play_loop()
{
  if(!m_omx_reader.Open(m_filename, IsURL(m_filename), m_dump_format, m_config_audio.is_live,
                        m_timeout, m_cookie, m_user_agent, m_lavfdopts, m_avdict, m_DvdPlayer))
    return exit_with_message("File read error or format not supported");

  if (m_dump_format_exit)
    return ABORT_PLAY;

  if(m_is_dvd)
  {
    printf("Playing: %s, Track: %d\n", m_filename.c_str(), m_track + 1);
    osd_printf(UM_NORM, "%s\nTrack %d", m_DvdPlayer->GetTitle().c_str(), m_track + 1);

    UpdateRaspicastMetaData(m_DvdPlayer->GetTitle() + " - Track " + to_string(m_track + 1));
  }
  else
  {
    printf("Playing: %s\n", m_filename.c_str());

    int lastSlash = m_filename.find_last_of('/');
    string short_filename = m_filename.substr(lastSlash + 1);
    uri_unescape(short_filename);

    osd_printf(UM_NORM, short_filename.c_str());
    UpdateRaspicastMetaData(short_filename);
  }

  // select an audio stream
  if(m_audio_lang[0] != '\0')
    m_audio_index = m_omx_reader.GetStreamByLanguage(OMXSTREAM_AUDIO, m_audio_lang);

  // Where no audio stream has been selected, use the first stream other than audio narrative
  if(m_audio_index == -1)
  {
    int audiostreamcount = m_omx_reader.AudioStreamCount();

    for(int i=0; i < audiostreamcount; i++)
    {
      std::string lang = m_omx_reader.GetStreamLanguage(OMXSTREAM_AUDIO, i);

      if(lang == "NAR")
      {
        printf("Avoiding audio description stream: %d\n", i + 1);
      }
      else
      {
        printf("Selecting audio stream: %d\n", i + 1);
        m_audio_index = i;
        break;
      }
    }
  }

  m_has_video     = m_omx_reader.VideoStreamCount();
  m_has_audio     = m_audio_index == -2 ? false : m_omx_reader.AudioStreamCount();
  m_has_subtitle  = m_has_external_subtitles || m_omx_reader.SubtitleStreamCount();
  m_loop          = m_loop && m_omx_reader.CanSeek();

  m_av_clock->OMXStateIdle();
  m_av_clock->OMXStop();
  m_av_clock->OMXPause();

  m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_config_audio.hints);
  m_omx_reader.GetHints(OMXSTREAM_VIDEO, m_config_video.hints);

  if (m_fps > 0.0f)
    m_config_video.hints.fpsrate = m_fps * AV_TIME_BASE, m_config_video.hints.fpsscale = AV_TIME_BASE;

  if(m_audio_index > -1)
    m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, m_audio_index);
          
  if(m_has_video && m_refresh)
  {
    memset(&tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
    vc_tv_get_display_state(&tv_state);

    SetVideoMode(m_config_video.hints.width, m_config_video.hints.height, m_config_video.hints.fpsrate, m_config_video.hints.fpsscale, m_3d);
  }

  // get display aspect
  TV_DISPLAY_STATE_T current_tv_state;
  memset(&current_tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
  vc_tv_get_display_state(&current_tv_state);
  if(current_tv_state.state & ( VC_HDMI_HDMI | VC_HDMI_DVI )) {
    //HDMI or DVI on
    m_config_video.display_aspect = get_display_aspect_ratio((HDMI_ASPECT_T)current_tv_state.display.hdmi.aspect_ratio);
  } else {
    //composite on
    m_config_video.display_aspect = get_display_aspect_ratio((SDTV_ASPECT_T)current_tv_state.display.sdtv.display_options.aspect);
  }
  m_config_video.display_aspect *= (float)current_tv_state.display.hdmi.height/(float)current_tv_state.display.hdmi.width;

  if (m_orientation >= 0)
    m_config_video.hints.orientation = m_orientation;

  if(m_has_video && !m_player_video.Open(m_av_clock, m_config_video))
    return exit_with_message("Failed to open video");

  if(m_has_subtitle || m_osd)
  {
    if(!m_has_external_subtitles) m_external_subtitles_path.clear();

    if(!m_player_subtitles.Open(m_omx_reader.SubtitleStreamCount(),
                                m_external_subtitles_path))
      return exit_with_message("Failed to open subtitles");

    // sub_dim and sub_aspect asre passed through FindDVDSubs in case
    // the subtitles have a different size
    Dimension sub_dim = {m_config_video.hints.width, m_config_video.hints.height};
    float sub_aspect = m_config_video.hints.aspect;
    uint32_t *palette = m_DvdPlayer ? m_DvdPlayer->getPalette() : NULL;
    if(m_omx_reader.FindDVDSubs(sub_dim, sub_aspect, &palette))
    {
      if(!m_player_subtitles.initDVDSubs(sub_dim, sub_aspect, m_config_video.aspectMode, palette))
        return exit_with_message("Failed to initialise DVD subtitles");
    }

    if(!m_DvdPlayer && palette != NULL)
      free(palette);
  }

  if(m_has_subtitle)
  {
    if(!m_has_external_subtitles && m_subtitle_lang[0] != '\0')
      m_subtitle_index = m_omx_reader.GetStreamByLanguage(OMXSTREAM_SUBTITLE, m_subtitle_lang);

    m_player_subtitles.SetActiveStream(m_subtitle_index);
  }

  m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_config_audio.hints);

  if (m_config_audio.device.empty())
  {
    if (vc_tv_hdmi_audio_supported(EDID_AudioFormat_ePCM, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) == 0)
      m_config_audio.device = "omx:hdmi";
    else
      m_config_audio.device = "omx:local";
  }

  if(m_config_audio.device == "omx:alsa" && m_config_audio.subdevice.empty())
    m_config_audio.subdevice = "default";

  if ((m_config_audio.hints.codec == AV_CODEC_ID_AC3 || m_config_audio.hints.codec == AV_CODEC_ID_EAC3) &&
      vc_tv_hdmi_audio_supported(EDID_AudioFormat_eAC3, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) != 0)
    m_config_audio.passthrough = false;

  if (m_config_audio.hints.codec == AV_CODEC_ID_DTS &&
      vc_tv_hdmi_audio_supported(EDID_AudioFormat_eDTS, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) != 0)
    m_config_audio.passthrough = false;

  if(m_has_audio && !m_player_audio.Open(m_av_clock, m_config_audio, &m_omx_reader))
    return exit_with_message("Failed to open audio out");

  if(m_has_audio)
  {
    m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
    if (m_Amplification)
      m_player_audio.SetDynamicRangeCompression(m_Amplification);
  }

  if (m_threshold < 0.0f)
    m_threshold = m_config_audio.is_live ? 0.7f : 0.2f;

  PrintSubtitleInfo();

  m_av_clock->OMXReset(m_has_video, m_has_audio);
  m_av_clock->OMXStateExecute();

  // forget seek time fo all files being played
  if(!m_is_dvd_device) m_file_store.forget(m_filename);

  int64_t last_check_time = 0;
  bool update = false;
  bool chapter_seek = false;
  int64_t startpts = 0;
  bool sentStarted = true;
  int64_t last_seek_pos   = 0.0f;

  while(!m_stopped)
  {
    int64_t now = OMXClock::GetAbsoluteClock();
    update = false;
    chapter_seek = false;
    if (last_check_time == 0 || last_check_time + 20000 <= now)
    {
      update = true;
      last_check_time = now;
    }

    if (update) {
      OMXControlResult result = m_control_err
                               ? (OMXControlResult)(m_keyboard.getEvent())
                               : m_omxcontrol.getEvent();

      switch(result.getKey())
      {
      case KeyConfig::ACTION_CHANGE_FILE:
        m_replacement_filename = result.getWinArg();

        // Entering a pipe: would make no sense here
        if(IsPipe(m_replacement_filename))
        {
          m_replacement_filename.clear();
          CLogLog(LOGDEBUG, "Providing a pipe via dbus is not supported.");
          break;
        }

        m_stopped = true;
        return END_PLAY;
      case KeyConfig::ACTION_DECREASE_SPEED:
        if(playspeed_current > 0)
          playspeed_current--;
        SetSpeed(playspeeds[playspeed_current]);
        osd_printf(UM_STDOUT, "Playspeed: %.3f", playspeeds[playspeed_current]/1000.0f);
        m_Pause = false;
        break;
      case KeyConfig::ACTION_INCREASE_SPEED:
        if(playspeed_current < playspeed_max)
          playspeed_current++;
        SetSpeed(playspeeds[playspeed_current]);
        osd_printf(UM_STDOUT, "Playspeed: %.3f", playspeeds[playspeed_current]/1000.0f);
        m_Pause = false;
        break;
      case KeyConfig::ACTION_STEP:
        m_av_clock->OMXStep();
        puts("Step");
        {
          unsigned t = (unsigned) (m_av_clock->OMXMediaTime()*1e-3);
          int dur = m_omx_reader.GetStreamLengthSeconds();
          osd_printf(UM_NORM, "Step\n%02d:%02d:%02d.%03d / %02d:%02d:%02d",
              (t/3600000), (t/60000)%60, (t/1000)%60, t%1000,
              (dur/3600), (dur/60)%60, dur%60);
        }
        break;
      case KeyConfig::ACTION_PREVIOUS_AUDIO:
        if(m_has_audio)
        {
          int new_index = m_omx_reader.GetAudioIndex() - 1;
          if(new_index < 0) new_index = m_omx_reader.AudioStreamCount() - 1;
          m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, new_index);
          strcpy(m_audio_lang, m_omx_reader.GetStreamLanguage(OMXSTREAM_AUDIO, new_index).c_str());
          osd_printf(UM_NORM, "Audio stream: %d %s", new_index + 1, m_audio_lang);
        }
        break;
      case KeyConfig::ACTION_NEXT_AUDIO:
        if(m_has_audio)
        {
          int new_index = m_omx_reader.GetAudioIndex() + 1;
          if(new_index >= m_omx_reader.AudioStreamCount()) new_index = 0;
          m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, new_index);
          strcpy(m_audio_lang, m_omx_reader.GetStreamLanguage(OMXSTREAM_AUDIO, new_index).c_str());
          osd_printf(UM_NORM, "Audio stream: %d %s", new_index + 1, m_audio_lang);
        }
        break;
      case KeyConfig::ACTION_PREVIOUS_CHAPTER:
        {
          int current_chapter = m_omx_reader.GetChapter();
          int total_chapters = m_omx_reader.GetChapterCount();

          if(current_chapter > -1 && total_chapters > 0)
          {
            int go_to_ch = current_chapter - 1;

            if(current_chapter == 0)
            {
              m_send_eos = true;
              m_next_prev_file = -1;
              return END_PLAY;
            }
            else if(m_omx_reader.SeekChapter(go_to_ch, &startpts))
            {
              osd_printf(UM_NORM, "Chapter %d", go_to_ch + 1);
              FlushStreams(startpts);
              m_seek_flush = true;
              chapter_seek = true;
            }
          }
          else
          {
            m_incr = -600;
          }
        }
        break;
      case KeyConfig::ACTION_NEXT_CHAPTER:
        {
          int current_chapter = m_omx_reader.GetChapter();
          int total_chapters = m_omx_reader.GetChapterCount();

          if(current_chapter > -1 && total_chapters > 0)
          {
            int go_to_ch = current_chapter + 1;

            if(go_to_ch >= total_chapters)
            {
              m_send_eos = true;
              m_next_prev_file = 1;
              return END_PLAY;
            }
            else if(m_omx_reader.SeekChapter(go_to_ch, &startpts))
            {
              osd_printf(UM_NORM, "Chapter %d", go_to_ch + 1);
              FlushStreams(startpts);
              m_seek_flush = true;
              chapter_seek = true;
            }
          }
          else
          {
            m_incr = 600;
          }
        }
        break;
      case KeyConfig::ACTION_PREVIOUS_FILE:
        m_next_prev_file = -1;
        return END_PLAY;
        break;
      case KeyConfig::ACTION_NEXT_FILE:
        m_next_prev_file = 1;
        return END_PLAY;
        break;
      case KeyConfig::ACTION_PREVIOUS_SUBTITLE:
        if(m_has_subtitle) ChangeSubtitle(-1);
        break;
      case KeyConfig::ACTION_NEXT_SUBTITLE:
        if(m_has_subtitle) ChangeSubtitle(+1);
        break;
      case KeyConfig::ACTION_TOGGLE_SUBTITLE:
        if(m_has_subtitle)
        {
          if(m_player_subtitles.GetVisible()) {
            m_player_subtitles.SetVisible(false);
            osd_print("Subtitles Off");
          } else {
            m_player_subtitles.SetVisible(true);
            osd_print("Subtitles On");
          }
          PrintSubtitleInfo();
        }
        break;
      case KeyConfig::ACTION_HIDE_SUBTITLES:
        if(m_has_subtitle)
        {
          m_player_subtitles.SetVisible(false);
          osd_print("Subtitles Off");
          PrintSubtitleInfo();
        }
        break;
      case KeyConfig::ACTION_SHOW_SUBTITLES:
        if(m_has_subtitle)
        {
          m_player_subtitles.SetVisible(true);
          osd_print("Subtitles On");
          PrintSubtitleInfo();
        }
        break;
      case KeyConfig::ACTION_DECREASE_SUBTITLE_DELAY:
        if(m_has_subtitle && m_player_subtitles.GetVisible())
        {
          int new_delay = m_player_subtitles.GetDelay() - 250;
          osd_printf(UM_NORM, "Subtitle delay: %d ms", new_delay);
          m_player_subtitles.SetDelay(new_delay);
          PrintSubtitleInfo();
        }
        break;
      case KeyConfig::ACTION_INCREASE_SUBTITLE_DELAY:
        if(m_has_subtitle && m_player_subtitles.GetVisible())
        {
          int new_delay = m_player_subtitles.GetDelay() + 250;
          osd_printf(UM_NORM, "Subtitle delay: %d ms", new_delay);
          m_player_subtitles.SetDelay(new_delay);
          PrintSubtitleInfo();
        }
        break;
      case KeyConfig::ACTION_EXIT:
        m_stopped = true;
        return END_PLAY;
        break;
      case KeyConfig::ACTION_SEEK_BACK_SMALL:
        if(m_omx_reader.CanSeek()) m_incr = -30;
        break;
      case KeyConfig::ACTION_SEEK_FORWARD_SMALL:
        if(m_omx_reader.CanSeek()) m_incr = 30;
        break;
      case KeyConfig::ACTION_SEEK_FORWARD_LARGE:
        if(m_omx_reader.CanSeek()) m_incr = 600;
        break;
      case KeyConfig::ACTION_SEEK_BACK_LARGE:
        if(m_omx_reader.CanSeek()) m_incr = -600;
        break;
      case KeyConfig::ACTION_SEEK_RELATIVE:
        m_incr = result.getArg() * 1e-6;
        break;
      case KeyConfig::ACTION_SEEK_ABSOLUTE:
        m_incr = (result.getArg() - m_av_clock->OMXMediaTime()) * 1e-6;
        break;
      case KeyConfig::ACTION_SET_ALPHA:
        m_player_video.SetAlpha(result.getArg());
        break;
      case KeyConfig::ACTION_SET_LAYER:
        m_player_video.SetLayer(result.getArg());
        break;
      case KeyConfig::ACTION_PLAY:
        PlayPause(false);
        break;
      case KeyConfig::ACTION_PAUSE:
        PlayPause(true);
        break;
      case KeyConfig::ACTION_PLAYPAUSE:
        PlayPause(!m_Pause);
        break;
      case KeyConfig::ACTION_HIDE_VIDEO:
        // set alpha to minimum
        m_player_video.SetAlpha(0);
        break;
      case KeyConfig::ACTION_UNHIDE_VIDEO:
        // set alpha to maximum
        m_player_video.SetAlpha(255);
        break;
      case KeyConfig::ACTION_SET_ASPECT_MODE:
        if (result.getWinArg()) {
          if (!strcasecmp(result.getWinArg(), "letterbox"))
            m_config_video.aspectMode = 1;
          else if (!strcasecmp(result.getWinArg(), "fill"))
            m_config_video.aspectMode = 2;
          else if (!strcasecmp(result.getWinArg(), "stretch"))
            m_config_video.aspectMode = 3;
          else
            m_config_video.aspectMode = 0;
          m_player_video.SetVideoRect(m_config_video.aspectMode);
        }
        break;
      case KeyConfig::ACTION_DECREASE_VOLUME:
        m_Volume -= 50;
        m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
        osd_printf(UM_STDOUT, "Volume: %.2f dB", m_Volume / 100.0f);
        break;
      case KeyConfig::ACTION_INCREASE_VOLUME:
        m_Volume += 50;
        m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
        osd_printf(UM_STDOUT, "Volume: %.2f dB", m_Volume / 100.0f);
        break;
      default:
        break;
      }
    }

    if(m_seek_flush || m_incr != 0)
    {
      int64_t seek_pos     = 0;
      int64_t pts          = 0;

      if(m_has_subtitle)
        m_player_subtitles.Pause();

      if (!chapter_seek)
      {
        pts = m_av_clock->OMXMediaTime();

        seek_pos = (pts ? pts : last_seek_pos) + (int64_t)m_incr * AV_TIME_BASE;
        last_seek_pos = seek_pos;

        if(m_omx_reader.SeekTime(seek_pos, m_incr < 0, &startpts))
        {
          int t = (int)(startpts*1e-6);
          int dur = m_omx_reader.GetStreamLengthSeconds();
          show_progress_message("Seek", t, dur, true);

          FlushStreams(startpts);
        }
      }

      sentStarted = false;

      if (m_omx_reader.IsEof())
        return END_PLAY;

      // Quick reset to reduce delay during loop & seek.
      if (m_has_video && !m_player_video.Reset())
        return exit_with_message("Failed to open video out");

      CLogLog(LOGDEBUG, "Seeked %.0f %lld %lld", (double)seek_pos/AV_TIME_BASE, startpts, m_av_clock->OMXMediaTime());

      m_av_clock->OMXPause();

      if(m_has_subtitle)
        m_player_subtitles.Resume();
      m_seek_flush = false;
      m_incr = 0;
    }

    /* player got in an error state */
    if(m_player_audio.Error())
      return exit_with_message("Audio player error");

    if (update)
    {
      /* when the video/audio fifos are low, we pause clock, when high we resume */
      int64_t stamp = m_av_clock->OMXMediaTime();
      int64_t audio_pts = m_player_audio.GetCurrentPTS();
      int64_t video_pts = m_player_video.GetCurrentPTS();

      float audio_fifo = audio_pts == AV_NOPTS_VALUE ? 0.0f : (audio_pts - stamp) * 1e-6;
      float video_fifo = video_pts == AV_NOPTS_VALUE ? 0.0f : (video_pts - stamp) * 1e-6;
      float threshold = std::min(0.1f, (float)m_player_audio.GetCacheTotal() * 0.1f);
      bool audio_fifo_low = false, video_fifo_low = false, audio_fifo_high = false, video_fifo_high = false;

      if(m_stats)
      {
        static int count;
        if ((count++ & 7) == 0)
          printf("M:%lld V:%6.2fs %6dk/%6dk A:%6.2f %6.02fs/%6.02fs Cv:%6uk Ca:%6uk                            \r", stamp,
               video_fifo, (m_player_video.GetDecoderBufferSize()-m_player_video.GetDecoderFreeSpace())>>10, m_player_video.GetDecoderBufferSize()>>10,
               audio_fifo, m_player_audio.GetDelay(), m_player_audio.GetCacheTotal(),
               m_player_video.GetCached()>>10, m_player_audio.GetCached()>>10);
      }

      if (audio_pts != AV_NOPTS_VALUE)
      {
        audio_fifo_low = m_has_audio && audio_fifo < threshold;
        audio_fifo_high = !m_has_audio || audio_fifo > m_threshold;
      }
      if (video_pts != AV_NOPTS_VALUE)
      {
        video_fifo_low = m_has_video && video_fifo < threshold;
        video_fifo_high = !m_has_video || video_fifo > m_threshold;
      }
      CLogLog(LOGDEBUG, "Normal M:%lld (A:%lld V:%lld) P:%d A:%.2f V:%.2f/T:%.2f (%d,%d,%d,%d) A:%d%% V:%d%% (%.2f,%.2f)", stamp, audio_pts, video_pts, m_av_clock->OMXIsPaused(),
        audio_pts == AV_NOPTS_VALUE ? 0.0:audio_fifo, video_pts == AV_NOPTS_VALUE ? 0.0:video_fifo, m_threshold, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high,
        m_player_audio.GetLevel(), m_player_video.GetLevel(), m_player_audio.GetDelay(), (float)m_player_audio.GetCacheTotal());

      // keep latency under control by adjusting clock (and so resampling audio)
      if (m_config_audio.is_live)
      {
        float latency = AV_NOPTS_VALUE;
        if (m_has_audio && audio_pts != AV_NOPTS_VALUE)
          latency = audio_fifo;
        else if (!m_has_audio && m_has_video && video_pts != AV_NOPTS_VALUE)
          latency = video_fifo;
        if (!m_Pause && latency != AV_NOPTS_VALUE)
        {
          if (m_av_clock->OMXIsPaused())
          {
            if (latency > m_threshold)
            {
              CLogLog(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_omx_reader.IsEof(), m_omx_pkt);
              m_av_clock->OMXResume();
              m_latency = latency;
            }
          }
          else
          {
            m_latency = m_latency*0.99f + latency*0.01f;
            float speed = 1.0f;
            if (m_latency < 0.5f*m_threshold)
              speed = 0.990f;
            else if (m_latency < 0.9f*m_threshold)
              speed = 0.999f;
            else if (m_latency > 2.0f*m_threshold)
              speed = 1.010f;
            else if (m_latency > 1.1f*m_threshold)
              speed = 1.001f;

            m_av_clock->OMXSetSpeed(S(speed));
            m_av_clock->OMXSetSpeed(S(speed), true, true);
            CLogLog(LOGDEBUG, "Live: %.2f (%.2f) S:%.3f T:%.2f", m_latency, latency, speed, m_threshold);
          }
        }
      }
      else if(!m_Pause && (m_omx_reader.IsEof() || m_omx_pkt || (audio_fifo_high && video_fifo_high)))
      {
        if (m_av_clock->OMXIsPaused())
        {
          CLogLog(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_omx_reader.IsEof(), m_omx_pkt);
          m_av_clock->OMXResume();
        }
      }
      else if (m_Pause || audio_fifo_low || video_fifo_low)
      {
        if (!m_av_clock->OMXIsPaused())
        {
          if (!m_Pause)
            m_threshold = std::min(2.0f*m_threshold, 16.0f);
          CLogLog(LOGDEBUG, "Pause %.2f,%.2f (%d,%d,%d,%d) %.2f", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_threshold);
          m_av_clock->OMXPause();
        }
      }
    }
    if (!sentStarted)
    {
      CLogLog(LOGDEBUG, "COMXPlayer::HandleMessages - player started RESET");
      m_av_clock->OMXReset(m_has_video, m_has_audio);
      sentStarted = true;
    }

    if(!m_omx_pkt)
      m_omx_pkt = m_omx_reader.Read();

    if(m_omx_pkt)
      m_send_eos = false;

    if(m_omx_reader.IsEof() && !m_omx_pkt)
    {
      if (!m_send_eos && m_has_video)
        m_player_video.SubmitEOS();
      if (!m_send_eos && m_has_audio)
        m_player_audio.SubmitEOS();
      m_send_eos = true;
      if ( (m_has_video && !m_player_video.IsEOS()) ||
           (m_has_audio && !m_player_audio.IsEOS()) )
      {
        OMXClock::OMXSleep(10);
        continue;
      }

      if (m_loop)
      {
        int64_t cur_pos = m_av_clock->OMXMediaTime();
        m_incr = m_loop_from - (cur_pos ? cur_pos : last_seek_pos) / AV_TIME_BASE;
        continue;
      }

      break;
    }

    if(!m_omx_pkt)
    {
      OMXClock::OMXSleep(10);
    }
    else if(m_omx_pkt->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if(m_has_video && m_omx_reader.IsActive(OMXSTREAM_VIDEO, m_omx_pkt->stream_index))
      {
        if(m_player_video.AddPacket(m_omx_pkt))
          m_omx_pkt = NULL;
        else
          OMXClock::OMXSleep(10);
      }
    }
    else if(m_omx_pkt->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      if(m_has_audio && playspeed_current == playspeed_normal)
      {
        if(m_player_audio.AddPacket(m_omx_pkt))
          m_omx_pkt = NULL;
        else
          OMXClock::OMXSleep(10);
      }
    }
    else if(m_omx_pkt->codec_type == AVMEDIA_TYPE_SUBTITLE)
    {
      if(m_has_subtitle && playspeed_current == playspeed_normal)
      {
        m_player_subtitles.AddPacket(m_omx_pkt,
                        m_omx_reader.GetRelativeIndex(m_omx_pkt->stream_index));
        m_omx_pkt = NULL;
      }
    }
    else
    {
      delete m_omx_pkt;
      m_omx_pkt = NULL;
    }
  }
  return END_PLAY;
}



void end_of_play_loop()
{
  if (m_stats)
    puts("");

  m_player_subtitles.Clear();

  int t = (int)(m_av_clock->OMXMediaTime()*1e-6);
  int dur = m_omx_reader.GetStreamLengthSeconds();
  printf("Stopped at: %02u:%02u:%02u\n", (t/3600), (t/60)%60, t%60);
  printf("  Duration: %02u:%02u:%02u\n", (dur/3600), (dur/60)%60, dur%60);

  // Catch eos errors, except for live streams
  if(!m_config_audio.is_live)
  {
    // Try to catch instances where m_send_eos has been set but we haven't
    // actually reached the end of the current file.
    if(m_send_eos && (dur - t) > 2)
      m_send_eos = false;

    // and instances where we're stopping after the end a file
    if(t >= dur)
      m_send_eos = true;
  }

  // remember this for later
  m_ext_subs_showing = m_has_external_subtitles && m_player_subtitles.GetVisible();

  // flush streams
  FlushStreams(AV_NOPTS_VALUE);
  m_omx_reader.Close();
  m_player_subtitles.Close();
  m_player_video.Close();
  m_player_audio.Close();

  // stop seeking
  m_seek_flush = false;
  m_incr = 0;
}


int playlist_control()
{
  int t = (int)(m_av_clock->OMXMediaTime()*1e-6);

  if(m_playlist_enabled) {
    if(!m_stopped && (m_send_eos || m_next_prev_file != 0)) {
      // default to playing next track file
      if(m_next_prev_file == 0) m_next_prev_file = 1;

      // if this is a DVD look for next track
      if(m_is_dvd) {
        if(m_DvdPlayer->ChangeTrack(m_next_prev_file, m_track))
        {
          m_firstfile = false;
          m_next_prev_file = 0;
          return RUN_PLAY_LOOP;
        }

        // no more tracks to play, exit DVD mode
        m_is_dvd = false;
        delete m_DvdPlayer;
        m_DvdPlayer = NULL;
      }

      // Play next file in playlist if there is one...
      // 'Exists' checks if file is readable
      if(!m_is_dvd_device && m_playlist.ChangeFile(m_next_prev_file, m_filename)
          && Exists(m_filename)) {
        m_firstfile = false;
        m_next_prev_file = 0;
        return CHANGE_PLAYLIST_ITEM;
      }
    } else if(!m_firstfile || t > 5) {
      if(m_is_dvd_device)
        m_dvd_store.remember(m_track, t, &m_audio_lang[0], &m_subtitle_lang[0]);
      else
        m_file_store.remember(m_filename, m_track, t, &m_audio_lang[0], &m_subtitle_lang[0],
            m_ext_subs_showing);
    }
  }

  if(!m_replacement_filename.empty()) {
    // we've received a new file to play via dbus
    if(m_is_dvd) {
      m_is_dvd = false;
      delete m_DvdPlayer;
      m_DvdPlayer = NULL;
    }

    m_filename = m_replacement_filename;
    m_replacement_filename.clear();

    m_has_external_subtitles = false;
    m_external_subtitles_path.clear();

    m_firstfile = true;

    return CHANGE_FILE;
  }

  return SHUTDOWN;
}

int shutdown(bool exit_with_error)
{
  // not playing anything else, so shutdown
  if (m_NativeDeinterlace)
  {
    char response[80];
    vc_gencmd(response, sizeof response, "hvs_update_fields %d", 0);
  }

  if(m_has_video && m_refresh && tv_state.display.hdmi.group && tv_state.display.hdmi.mode)
  {
    vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_HDMI, (HDMI_RES_GROUP_T)tv_state.display.hdmi.group, tv_state.display.hdmi.mode);
  }

  delete m_av_clock;

  if(m_DvdPlayer)
    delete m_DvdPlayer;

  g_OMX.Deinitialize();
  bcm_host_deinit();

  // save recent files
  if(m_playlist_enabled) {
    if(m_is_dvd_device) m_dvd_store.saveStore();
    else m_file_store.saveStore();
  }

  puts("have a nice day ;)");

  // Exit on failure
  if(exit_with_error)
    return EXIT_FAILURE;

  // If user has chosen to dump format exit with sucess
  if(m_dump_format_exit)
    return EXIT_SUCCESS;

  // exit status OMXPlayer defined value on user quit
  // (including a stop caused by SIGTERM or SIGINT)
  if(m_stopped) {
    puts("Stopped before end of file");
    return PLAY_STOPPED;
  }

  // exit status success on playback end
  if(m_send_eos) {
    puts("Reached end of file");
    return EXIT_SUCCESS;
  }

  // exit status failure on other cases
  return EXIT_FAILURE;
}
