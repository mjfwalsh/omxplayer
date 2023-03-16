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

#include "utils/log.h"

#include "OMXVideo.h"
#include "OMXClock.h"
#include "OMXAudio.h"
#include "OMXReader.h"
#include "OMXPlayerVideo.h"
#include "OMXPlayerAudio.h"
#include "OMXPlayerSubtitles.h"
#include "OMXDvdPlayer.h"
#include "OMXControl.h"
#include "KeyConfig.h"
#include "Keyboard.h"
#include "utils/RegExp.h"
#include "AutoPlaylist.h"
#include "RecentFileStore.h"
#include "RecentDVDStore.h"
#include "utils/misc.h"
#include "VideoCore.h"
#include "DbusCommandSearch.h"
#include "omxplayer.h"
#include "DispmanxLayer.h"
#include "CECListener.h"

#include <string>

#include "version.h"

volatile sig_atomic_t m_stopped           = false;
long              m_Volume              = 0;
long              m_Amplification       = 0;
bool              m_NativeDeinterlace   = false;
bool              m_osd                 = true;
std::string       m_external_subtitles_path;
bool              m_cmd_line_subtitles  = false;
bool              m_Pause               = false;
OMXReader         *m_omx_reader         = NULL;
int               m_audio_index         = -1;
OMXClock          *m_av_clock           = NULL;
OMXControl        m_omxcontrol;
Keyboard          *m_keyboard           = NULL;
OMXAudioConfig    m_config_audio;
OMXVideoConfig    m_config_video;
OMXPacket         *m_omx_pkt            = NULL;
int               m_subtitle_index      = -1;
OMXPlayerVideo    *m_player_video       = NULL;
OMXPlayerAudio    *m_player_audio       = NULL;
OMXPlayerSubtitles *m_player_subtitles  = NULL;
bool              m_has_subtitle        = false;
bool              m_loop                = false;
RecentFileStore   m_file_store;
RecentDVDStore    m_dvd_store;
AutoPlaylist      m_playlist;
bool              m_firstfile           = true;
bool              m_send_eos            = false;
std::string       m_filename;
int               m_track               = -1;
bool              m_is_dvd_device       = false;
OMXDvdPlayer      *m_DvdPlayer          = NULL;
int               m_incr                = -1;
int               m_loop_from           = 0;
bool              m_stats               = false;
bool              m_dump_format         = false;
bool              m_dump_format_exit    = false;
FORMAT_3D_T       m_3d                  = CONF_FLAGS_FORMAT_NONE;
bool              m_refresh             = false;
float             m_threshold           = -1.0f; // amount of audio/video required to come out of buffering
int               m_orientation         = -1; // unset
float             m_fps                 = 0.0f; // unset
int               m_next_prev_file      = 0;
char              m_audio_lang[4]       = "\0";
char              m_subtitle_lang[4]    = "\0";
std::string       m_replacement_filename;
bool              m_playlist_enabled    = true;
float             m_latency             = 0.0f;
VideoCore         m_video_core;
CECListener       m_cec_listener;

#define safe_delete(object) if(object) { delete object; object = NULL; }

float playspeeds[] = {0, 1/16.0, 1/8.0, 1/4.0, 1/2.0, 0.975, 1.0, 1.125, 2.0, 4.0};
const int playspeed_max = 9, playspeed_normal = 6;
int playspeed_current = playspeed_normal;

enum{ERROR=-1,SUCCESS,ONEBYTE};

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
  puts("usage: omxplayer [file|url]");
}

void print_version()
{
  printf("omxplayer - Commandline multimedia player for the Raspberry Pi\n");
  printf("        Build date: %s\n", VERSION_DATE);
  printf("        Version   : %s [%s]\n", VERSION_HASH, VERSION_BRANCH);
  printf("        Repository: %s\n", VERSION_REPO);
}

#define UM_STDOUT 0b00000001
#define UM_LONG   0b00000010
#define UM_SLEEP  0b00000100
#define UM_NORM   0b00000000
#define UM_ALL    0b00000111

void osd_print(int options, const char *msg)
{
  if(m_osd)
    m_player_subtitles->DisplayText(msg, (options & UM_LONG) ? 3000 : 1500, (options & UM_SLEEP));

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
    char buffer[120];
    va_list va;
    va_start(va, format);
    vsnprintf(buffer, 120, format, va);
    va_end(va);

    osd_print(options, &buffer[0]);
}

void show_progress_message(const char *msg, int pos)
{
  int dur = m_omx_reader->GetStreamLengthSeconds();
  osd_printf(UM_STDOUT, "%s\n%02d:%02d:%02d / %02d:%02d:%02d",
                msg, (pos/3600), (pos/60)%60, pos%60, (dur/3600), (dur/60)%60, dur%60);
}

std::string getShortFileName()
{
  int lastSlash = m_filename.find_last_of('/');
  std::string short_filename = m_filename.substr(lastSlash + 1);
  uri_unescape(short_filename);
  for(unsigned int i = 0; i < short_filename.length(); i++)
    if(short_filename[i] == '_') short_filename[i] = ' ';
  return short_filename;
}

static void UpdateRaspicastMetaData(const std::string &msg)
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
  int count = m_player_subtitles->GetStreamCount();
  int index = m_player_subtitles->GetActiveStream();

  printf("Subtitle count: %d, state: %s, index: %d, delay: %d\n",
         count,
         m_has_subtitle && m_player_subtitles->GetVisible() ? " on" : "off",
         index+1,
         m_has_subtitle ? m_player_subtitles->GetDelay() : 0);
}

static void SetSpeed(float iSpeed)
{
  m_omx_reader->SetSpeed(iSpeed);
  m_av_clock->SetSpeed(iSpeed);
}

static void FlushStreams(int64_t pts = AV_NOPTS_VALUE)
{
  m_av_clock->Stop();
  m_av_clock->Pause();

  if(m_player_video)
    m_player_video->Reset();

  if(m_player_audio)
    m_player_audio->Flush();

  if(pts != AV_NOPTS_VALUE)
  {
    m_av_clock->SetMediaTime(pts);
    m_av_clock->Pause();
    m_av_clock->Reset(m_player_video, m_player_audio);
  }

  if(m_has_subtitle)
    m_player_subtitles->Flush();

  if(m_omx_pkt)
  {
    delete m_omx_pkt;
    m_omx_pkt = NULL;
  }
}

void Seek(int seconds_delta)
{
  // dvds can only seek to the start of chapters
  if(m_DvdPlayer) return;

  int64_t cur_pts = m_av_clock->GetMediaTime();
  int64_t seek_pts = cur_pts + (int64_t)seconds_delta * AV_TIME_BASE;

  if(m_omx_reader->SeekTime(seek_pts, &cur_pts))
  {
    show_progress_message("Seek", (int)(cur_pts * 1e-6));
    FlushStreams(cur_pts);
    CLogLog(LOGDEBUG, "Seeked %lld", cur_pts);
  }
}

// find the nearest element of the playspeeds array
// to the inputted play speed
int get_approx_speed(double &new_speed)
{
  const int arr_len = sizeof(playspeeds) / sizeof(int);

  for(int i = 0; i < arr_len - 1; i++)  
  {
    float midpoint = (playspeeds[i] + playspeeds[i+1]) / 2.0;
    if(new_speed < midpoint)
    {
      new_speed = playspeeds[i];
      return i;
    }
  }
  new_speed = playspeeds[arr_len - 1];
  return arr_len - 1;
}

int startup(int argc, char *argv[])
{
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGUSR1, sig_handler);

  // do we have enough memory to run
  int gpu_mem = m_video_core.get_mem_gpu();
  const int min_gpu_mem = 64;
  if (gpu_mem > 0 && gpu_mem < min_gpu_mem)
    printf("Only %dM of gpu_mem is configured. Try running \"sudo raspi-config\" and ensure that \"memory_split\" has a value of %d or greater\n", gpu_mem, min_gpu_mem);

  const int font_size_opt   = 0x101;
  const int align_opt       = 0x102;
  const int no_ghost_box_opt = 0x203;
  const int subtitles_opt   = 0x103;
  const int lines_opt       = 0x104;
  const int pos_opt         = 0x105;
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
  const int crop_opt        = 0x213;
  const int http_cookie_opt = 0x300;
  const int http_user_agent_opt = 0x301;
  const int lavfdopts_opt   = 0x400;
  const int avdict_opt      = 0x401;
  const int track_opt       = 0x402;
  const int start_paused_opt = 0x403;
  const int ffmpeg_log_level = 0x404;
  const int omxplayer_log_level = 0x405;

  struct option longopts[] = {
    { "info",         no_argument,        NULL,          'i' },
    { "with-info",    no_argument,        NULL,          'I' },
    { "help",         no_argument,        NULL,          'h' },
    { "version",      no_argument,        NULL,          'v' },
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
    { "win",          required_argument,  NULL,          pos_opt },
    { "crop",         required_argument,  NULL,          crop_opt },
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
    { "ffmpeg-log",   required_argument,  NULL,          ffmpeg_log_level },
    { "log",          required_argument,  NULL,          omxplayer_log_level },
    { 0, 0, 0, 0 }
  };

  int               c;
  std::string       mode;
  bool              centered            = false;
  bool              ghost_box           = true;
  unsigned int      subtitle_lines      = 3;
  float             font_size           = 0.055f;
  bool              no_hdmi_clock_sync  = false;
  uint32_t          background          = 0;
  const char        *keymap_file        = NULL;
  int               log_level           = LOGNONE;
  const char        *log_file           = NULL;
  bool              use_key_ctrl        = true;
  const char        *dbus_name          = "org.mpris.MediaPlayer2.omxplayer";

  while ((c = getopt_long(argc, argv, "awiIhvn:l:o:slb::pd3:Myzt:rg", longopts, NULL)) != -1)
  {
    switch (c) 
    {
      case 'r':
        m_refresh = true;
        break;
      case 'g':
        {
          log_file = optarg ? optarg : "./omxplayer.log";

          if(log_level == LOGNONE)
            log_level = LOGDEBUG;
        }
        break;
      case omxplayer_log_level:
        if(strcmp("none", optarg) == 0)
          log_level = LOGNONE;
        else if(strcmp("fatal", optarg) == 0)
          log_level = LOGFATAL;
        else if(strcmp("severe", optarg) == 0)
          log_level = LOGSEVERE;
        else if(strcmp("error", optarg) == 0)
          log_level = LOGERROR;
        else if(strcmp("warning", optarg) == 0)
          log_level = LOGWARNING;
        else if(strcmp("notice", optarg) == 0)
          log_level = LOGNOTICE;
        else if(strcmp("info", optarg) == 0)
          log_level = LOGINFO;
        else if(strcmp("debug", optarg) == 0)
          log_level = LOGDEBUG;
        else
          return EXIT_FAILURE;
        break;
      case ffmpeg_log_level:
        {
          int level = 0;
          if(optarg[0] >= '0' && optarg[0] <= '9' )
            level = atoi(optarg);
          else if(strcmp("quiet", optarg) == 0)
            level = AV_LOG_QUIET;
          else if(strcmp("panic", optarg) == 0)
            level = AV_LOG_PANIC;
          else if(strcmp("fatal", optarg) == 0)
            level = AV_LOG_FATAL;
          else if(strcmp("error", optarg) == 0)
            level = AV_LOG_ERROR;
          else if(strcmp("warning", optarg) == 0)
            level = AV_LOG_WARNING;
          else if(strcmp("info", optarg) == 0)
            level = AV_LOG_INFO;
          else if(strcmp("verbose", optarg) == 0)
            level = AV_LOG_VERBOSE;
          else if(strcmp("debug", optarg) == 0)
            level = AV_LOG_DEBUG;
          else if(strcmp("trace", optarg) == 0)
            level = AV_LOG_TRACE;
          else
            return EXIT_FAILURE;
          av_log_set_level(level);
        }
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
          puts("Valid options for 3d mode are TB, FP, or SBS");
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
        use_key_ctrl = false;
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
        m_cmd_line_subtitles = true;

        // check if command line provided subtitles file exists
        if(!Exists(m_external_subtitles_path))
        {
          printf("File \"%s\" not found.", m_external_subtitles_path.c_str());
          return EXIT_FAILURE;
        }

        break;
      case lines_opt:
        subtitle_lines = std::max(atoi(optarg), 1);
        break;
      case pos_opt:
        {
          int x1, x2, y1, y2;
          if(sscanf(optarg, "%d %d %d %d", &x1, &y1, &x2, &y2) == 4 ||
             sscanf(optarg, "%d,%d,%d,%d", &x1, &y1, &x2, &y2) == 4)
          {
            m_config_video.dst_rect.x = x1;
            m_config_video.dst_rect.y = y1;
            m_config_video.dst_rect.width = x2 - x1;
            m_config_video.dst_rect.height = y2 - y1;
          }
        }
        break;
      case crop_opt:
        {
          int x1, x2, y1, y2;
          if(sscanf(optarg, "%d %d %d %d", &x1, &y1, &x2, &y2) == 4 ||
             sscanf(optarg, "%d,%d,%d,%d", &x1, &y1, &x2, &y2) == 4)
          {
            m_config_video.src_rect.x = x1;
            m_config_video.src_rect.y = y1;
            m_config_video.src_rect.width = x2 - x1;
            m_config_video.src_rect.height = y2 - y1;
          }
        }
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
        OMXReader::SetDefaultTimeout(atof(optarg));
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
        const int size = sizeof(layouts) / sizeof(*layouts);
        int i;

        for(i=0; i < size; i++)
        {
          if (strcmp(optarg, layouts[i]) == 0)
          {
            m_config_audio.layout = (enum PCMLayout)i;
            break;
          }
        }
        if (i == size)
        {
          printf("Invalid layout specified: %s\n", optarg);
          puts("Valid options are: 2.0, 2.1, 3.0, 3.1, 4.0, 4.1, 5.0, 5.1, 7.0, and 7.1");
          return EXIT_FAILURE;
        }
        break;
      }
      case dbus_name_opt:
        dbus_name = optarg;
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
        OMXReader::SetCookie(optarg);
        break;
      case http_user_agent_opt:
        OMXReader::SetUserAgent(optarg);
        break;    
      case lavfdopts_opt:
        OMXReader::SetLavDopts(optarg);
        break;
      case avdict_opt:
        OMXReader::SetAvDict(optarg);
        break;
      case track_opt:
        m_track = atoi(optarg) - 1;
        if(m_track < 0) m_track = -1;
      case start_paused_opt:
        m_Pause = true;
        break;
      case 'h':
        print_usage();
        return EXIT_SUCCESS;
        break;
      case 'v':
        print_version();
        return EXIT_SUCCESS;
        break;
      case ':':
      case '?':
      default:
        return EXIT_FAILURE;
        break;
    }
  }

  if (optind >= argc) {
    print_usage();
    return EXIT_FAILURE;
  }

  // get filename
  m_filename = argv[optind];

  // start logging
  CLogInit(log_level, log_file);

  // start omx
  static COMXCore OMX;

  // start the clock
  static OMXClock clock;
  m_av_clock = &clock;

  // Open display
  DispmanxLayer::openDisplay(m_config_video.display, m_config_video.layer, m_config_video.dst_rect);

  // blank background - exclude fully transparent backgrounds
  if(background > 0x00FFFFFF)
  {
    static DispmanxLayer background_layer(4, Rect(0, 0, 0, 0), Dimension(1, 1));
    background_layer.setImageData(&background, false);
  }

  // init subtitle object
  static OMXPlayerSubtitles player_subs(font_size, centered, ghost_box, subtitle_lines, m_av_clock);
  m_player_subtitles = &player_subs;

  osd_print("Loading...");

  m_omxcontrol.connect(dbus_name);

  // 3d modes don't work without switch hdmi mode
  if (m_3d != CONF_FLAGS_FORMAT_NONE || m_NativeDeinterlace)
    m_refresh = true;

  // you really don't want to match refresh rate without hdmi clock sync
  if ((m_refresh || m_NativeDeinterlace) && !no_hdmi_clock_sync)
    m_config_video.hdmi_clock_sync = true;

  if(m_config_video.hdmi_clock_sync && !m_av_clock->HDMIClockSync())
    return EXIT_FAILURE;

  // disable keys when using stdin for input
  if(m_filename == "pipe:" || m_filename == "pipe:0")
    use_key_ctrl = false;

  if(use_key_ctrl)
  {
    static Keyboard keys(keymap_file);
    m_keyboard = &keys;
  }

  // Disable seeking and playlists when reading from a pipe
  if(IsPipe(m_filename))
  {
    m_config_audio.is_live = true;
    m_playlist_enabled = false;
  }

  // set default buffer threshold depending on whether we're playing a
  // live stream
  if (m_threshold < 0.0f)
    m_threshold = m_config_audio.is_live ? 0.7f : 0.2f;

  // no audio device name has been set on command line
  if(m_config_audio.device.empty())
    m_config_audio.device = m_video_core.getAudioDevice();

  // set defaults
  if(m_config_audio.device == "omx:alsa" && m_config_audio.subdevice.empty())
    m_config_audio.subdevice = "default";

  return CHANGE_FILE;
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
    {
      osd_printf(UM_ALL, "File \"%s\" not found.", getShortFileName().c_str());
      return END_PLAY_WITH_ERROR;
    }

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
      m_file_store.readlink(m_filename, m_track, m_incr,
                            &m_audio_lang[0], m_audio_index,
                            &m_subtitle_lang[0], m_subtitle_index);
      
      is_local_file = !IsURL(m_filename) && !IsPipe(m_filename);

      if(is_local_file && !Exists(m_filename))
      {
        osd_printf(UM_ALL, "File \"%s\" not found.", getShortFileName().c_str());
        return END_PLAY_WITH_ERROR;
      }
    }
  }

  // m_filename may have changed
  m_is_dvd_device = false;
  if(is_local_file)
  {
    // Are we dealing with a DVD VIDEO_TS folder or a device file
    CRegExp findvideots("^(.*?/VIDEO_TS|/dev/.*$)");
    if(findvideots.RegFind(m_filename, 0) > -1)
    {
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

      if(m_playlist_enabled && !started_from_link)
        m_file_store.retrieveRecentInfo(m_filename, m_track, m_incr,
                                        &m_audio_lang[0], m_audio_index,
                                        &m_subtitle_lang[0], m_subtitle_index);
    }
  }

  return CHANGE_PLAYLIST_ITEM;
}


// we jump here when playing the next item in an auto generated playlist
int change_playlist_item()
{
  std::string fileExt = m_filename.substr(m_filename.size()-4, 4);

  // a playlist item is a new file that could be an iso or a dmg files
  if(m_is_dvd_device || fileExt == ".iso" || fileExt == ".dmg")
  {
    // external subs are not supported for DVDs
    m_cmd_line_subtitles = false;
    m_external_subtitles_path.clear();

    // try to open the DVD
    try {
        m_DvdPlayer = new OMXDvdPlayer(m_filename);
    }
    catch(const char *msg) {
        puts(msg);
        return END_PLAY_WITH_ERROR;
    }

    // Was DVD played before?
    if(!m_dump_format_exit && m_is_dvd_device && m_playlist_enabled)
    {
      m_dvd_store.setCurrentDVD(m_DvdPlayer->GetID(), m_track, m_incr, &m_audio_lang[0], &m_subtitle_lang[0]);
    }

    // If m_track is set to -1, look for the first enabled track
    if(m_track == -1)
      m_track = 0;

    if(!m_DvdPlayer->OpenTrack(m_track))
    {
      puts("Failed to open DVD track");
      return END_PLAY_WITH_ERROR;
    }
  }
  else
  {
    // not a dvd so ignore any track variable
    m_track = -1;

    // and check for external subs
    if(m_osd && !m_cmd_line_subtitles && !IsURL(m_filename))
    {
      std::string subtitles_path = m_filename.substr(0, m_filename.find_last_of(".")) + ".srt";

      if(Exists(subtitles_path))
      {
        m_external_subtitles_path = subtitles_path;
      }
    }
  }

  // Start from beginning
  if(m_incr == -1)
    m_incr = 0;

  return RUN_PLAY_LOOP;
}


enum ControlFlow handle_event(enum Action search_key, DMessage *m)
{
  switch(search_key)
  {
  case OPEN_URI:
    {
      const char *file;
      if(!m->get_arg_string(&file))
      {
        m->respond_invalid_args();
        break;
      }

      m_replacement_filename.assign(file);

      // Entering a pipe: would make no sense here
      if(IsPipe(m_filename) || IsPipe(m_replacement_filename))
      {
        m_replacement_filename.clear();
        m->respond_invalid_args();
        CLogLog(LOGDEBUG, "Providing a pipe or replacing one via dbus is not supported.");
        break;
      }

      // error management
      m->respond_string(file);
      m_stopped = true;
      return END_PLAY;
    }

  case SET_SPEED:
  case ACTION_DECREASE_SPEED:
  case ACTION_INCREASE_SPEED:
    if(search_key == SET_SPEED)
    {
      double rate;
      if(!m->get_arg_double(&rate))
      {
        m->respond_invalid_args();
        break;
      }

      int new_speed = get_approx_speed(rate);
      m->respond_double(rate);

      if(new_speed == 0)
        return handle_event(ACTION_PAUSE, m);
      else
        playspeed_current = new_speed;
    }
    else if(search_key == ACTION_DECREASE_SPEED)
    {
      if(playspeed_current > 1) playspeed_current--;
    }
    else
    {
      if(playspeed_current < playspeed_max) playspeed_current++;
    }

    SetSpeed(playspeeds[playspeed_current]);
    osd_printf(UM_STDOUT, "Playspeed: %.3f", playspeeds[playspeed_current]);
    m_Pause = false;
    break;

  case ACTION_STEP:
    {
      m_av_clock->Step();
      int t = m_av_clock->GetMediaTime() * 1e-3;
      show_progress_message("Step", t);
    }
    break;

  case ACTION_PREVIOUS_AUDIO:
  case ACTION_NEXT_AUDIO:
  case SET_AUDIO_STREAM:
    if(search_key == SET_AUDIO_STREAM)
    {
      int index;
      if (!m->get_arg_int(&index))
      {
        m->respond_bool(false);
        break;
      }

      m_audio_index = m_player_audio->SetActiveStream(index);
      m->respond_bool(m_audio_index == index ? 1 : 0);
    }
    else
    {
      int delta = search_key == ACTION_NEXT_AUDIO ? 1 : -1;
      m_audio_index = m_player_audio->SetActiveStreamDelta(delta);
    }

    strcpy(m_audio_lang, m_omx_reader->GetStreamLanguage(OMXSTREAM_AUDIO, m_audio_index).c_str());
    if(m_audio_lang[0] == '\0')
      osd_printf(UM_NORM, "Audio stream: %d", m_audio_index + 1);
    else
      osd_printf(UM_NORM, "Audio stream: %d (%s)", m_audio_index + 1, m_audio_lang);
    break;

  case ACTION_PREVIOUS_CHAPTER:
  case ACTION_NEXT_CHAPTER:
    {
      int64_t cur_pts = m_av_clock->GetMediaTime();
      int ch = search_key == ACTION_NEXT_CHAPTER ? 1 : -1;

      switch(m_omx_reader->SeekChapter(ch, cur_pts))
      {
      case OMXReader::SEEK_SUCCESS:
        osd_printf(UM_NORM, "Chapter %d", ch + 1);
        FlushStreams(cur_pts);
        break;
      case OMXReader::SEEK_OUT_OF_BOUNDS:
        m_send_eos = true;
        m_next_prev_file = ch;
        return END_PLAY;
      case OMXReader::SEEK_NO_CHAPTERS:
        Seek(ch * 600);
        break;
      case OMXReader::SEEK_ERROR:
        break;
      }
    }
    break;

  case ACTION_PREVIOUS_FILE:
    m_next_prev_file = -1;
    return END_PLAY;
    break;

  case ACTION_NEXT_FILE:
    m_next_prev_file = 1;
    return END_PLAY;
    break;

  case ACTION_PREVIOUS_SUBTITLE:
  case ACTION_NEXT_SUBTITLE:
  case SET_SUBTITLE_STREAM:
    if(m_has_subtitle)
    {
      if(search_key == SET_SUBTITLE_STREAM)
      {
        int index;
        if(!m->get_arg_int(&index))
        {
          m->respond_bool(false);
          break;
        }

        m_subtitle_index = m_player_subtitles->SetActiveStream(index);
        m->respond_bool(m_subtitle_index == index ? 1 : 0);
      }
      else
      {
        int delta = search_key == ACTION_PREVIOUS_SUBTITLE ? -1 : 1;
        m_subtitle_index = m_player_subtitles->SetActiveStreamDelta(delta);
      }

      if(m_subtitle_index == -1) {
        m_subtitle_lang[0] = '\0';
        osd_print("Subtitles Off");
      } else if(m_subtitle_index == m_omx_reader->SubtitleStreamCount()) {
        m_subtitle_lang[0] = '\0';
        osd_print("Subtitle stream: External");
      } else {
        strcpy(m_subtitle_lang, m_omx_reader->GetStreamLanguage(OMXSTREAM_SUBTITLE,
            m_subtitle_index).c_str());
        if(m_subtitle_lang[0] == '\0')
          osd_printf(UM_NORM, "Subtitle stream: %d", m_subtitle_index + 1);
        else
          osd_printf(UM_NORM, "Subtitle stream: %d (%s)", m_subtitle_index + 1, m_subtitle_lang);
      }
      PrintSubtitleInfo();
    }
    break;

  case ACTION_TOGGLE_SUBTITLE:
  case ACTION_HIDE_SUBTITLES:
  case ACTION_SHOW_SUBTITLES:
    if(m_has_subtitle)
    {
      bool new_visible = search_key == ACTION_TOGGLE_SUBTITLE ?
        !m_player_subtitles->GetVisible()
          :
        search_key == ACTION_SHOW_SUBTITLES;

      m_player_subtitles->SetVisible(new_visible);
      osd_print(new_visible ? "Subtitles On" : "Subtitles Off");
      PrintSubtitleInfo();
    }
    break;

  case ACTION_DECREASE_SUBTITLE_DELAY:
    if(m_has_subtitle && m_player_subtitles->GetVisible())
    {
      int new_delay = m_player_subtitles->GetDelay() - 250;
      osd_printf(UM_NORM, "Subtitle delay: %d ms", new_delay);
      m_player_subtitles->SetDelay(new_delay);
      PrintSubtitleInfo();
    }
    break;

  case ACTION_INCREASE_SUBTITLE_DELAY:
    if(m_has_subtitle && m_player_subtitles->GetVisible())
    {
      int new_delay = m_player_subtitles->GetDelay() + 250;
      osd_printf(UM_NORM, "Subtitle delay: %d ms", new_delay);
      m_player_subtitles->SetDelay(new_delay);
      PrintSubtitleInfo();
    }
    break;

  case ACTION_EXIT:
    m_stopped = true;
    return END_PLAY;
    break;

  case ACTION_SEEK_BACK_SMALL:
    Seek(-30);
    break;

  case ACTION_SEEK_FORWARD_SMALL:
    Seek(30);
    break;

  case ACTION_SEEK_FORWARD_LARGE:
    Seek(600);
    break;

  case ACTION_SEEK_BACK_LARGE:
    Seek(-600);
    break;

  case ACTION_PLAY:
  case ACTION_PAUSE:
  case ACTION_PLAYPAUSE:
    {
      m_Pause = search_key == ACTION_PLAYPAUSE ?
        !m_Pause
          :
        search_key == ACTION_PAUSE;

      if (m_av_clock->PlaySpeed() != DVD_PLAYSPEED_NORMAL &&
          m_av_clock->PlaySpeed() != DVD_PLAYSPEED_PAUSE)
      {
        playspeed_current = playspeed_normal;
        SetSpeed(playspeeds[playspeed_normal]);
      }

      if(m_has_subtitle)
      {
        if(m_Pause) m_player_subtitles->Pause();
        else m_player_subtitles->Resume();
      }

      int t = m_av_clock->GetMediaTime() * 1e-6;
      show_progress_message(m_Pause ? "Pause" : "Play", t);
    }
    break;

  case ACTION_HIDE_VIDEO:
    // set alpha to minimum
    if(m_player_video) m_player_video->SetAlpha(0);
    break;

  case ACTION_UNHIDE_VIDEO:
    // set alpha to maximum
    if(m_player_video) m_player_video->SetAlpha(255);
    break;

  case ACTION_DECREASE_VOLUME:
  case ACTION_INCREASE_VOLUME:
    if(m_player_audio)
    {
      m_Volume += search_key == ACTION_INCREASE_VOLUME ? 50 : -50;
      m_player_audio->SetVolume(pow(10, m_Volume / 2000.0));
      osd_printf(UM_STDOUT, "Volume: %.2f dB", m_Volume / 100.0f);
    }
    break;

  case INVALID_METHOD:
    m->respond_unknown_method();
    break;

  case INVALID_PROPERTY:
    m->respond_unknown_property();
    break;

  case RAISE:
    m->needs_response = false;
    break;

  case GET:
    {
      //Retrieve interface and property name
      const char *property;
      if(m->ignore_arg() && m->get_arg_string(&property))
      {
        return handle_event(dbus_find_property(property), m);
      }
      else
      {
         m->respond_invalid_args();
         break;
      }
    }

  case SET:
    //Retrieve interface, property name and value
    //Message has the form message[STRING:interface STRING:property DOUBLE:value] or message[STRING:interface STRING:property VARIANT[DOUBLE:value]]
    const char *property;

    if(!m->ignore_arg() || !m->get_arg_string(&property))
    {
      m->respond_invalid_args();
      break;
    }

    if(strcmp(property, "Volume")==0)
    {
      return handle_event(SET_VOLUME, m);
    }
    else if (strcmp(property, "Rate")==0)
    {
      return handle_event(SET_SPEED, m);
    }

    //Wrong property
    m->respond_unknown_property();
    break;

  case CAN_QUIT:
  case CAN_GO_FULLSCREEN:
  case CAN_CONTROL:
  case CAN_PLAY:
  case CAN_PAUSE:
  case GET_FULLSCREEN:
    m->respond_bool(true);
    break;

  case CAN_SET_FULLSCREEN:
  case CAN_RAISE:
  case HAS_TRACK_LIST:
  case CAN_GO_NEXT:
  case CAN_GO_PREVIOUS:
  case GET_CAN_RAISE:
  case GET_HAS_TRACK_LIST:
    m->respond_bool(false);
    break;

  case GET_IDENTITY:
    m->respond_string("OMXPlayer");
    break;

  case GET_SUPPORTED_URI_SCHEMES:
    {
      const char *UriSchemes[] = {"file", "http", "rtsp", "rtmp"};
      m->respond_array(UriSchemes, 4);
      break;
    }

  case GET_SUPPORTED_MIME_TYPES:
    {
      const char *MimeTypes[] = {}; // Needs supplying
      m->respond_array(MimeTypes, 0);
      break;
    }

  case CAN_SEEK:
    m->respond_bool(m_omx_reader->CanSeek());
    break;

  case GET_PLAYBACK_STATUS:
    m->respond_string(m_av_clock->IsPaused() ? "Paused" : "Playing");
    break;

  case GET_SOURCE:
    m->respond_string(m_filename.c_str());
    break;

  case SET_VOLUME:
    {
      if(!m_player_audio)
      {
        m->respond_double(0.0);
        break;
      }

      double volume;
      if(m->get_arg_double(&volume))
      {
        if(volume < 0.0) volume = 0.0;
        m->respond_double(volume);
        m_Volume = 2000 * log10(volume);
        m_player_audio->SetVolume(volume);
        osd_printf(UM_STDOUT, "Volume: %.2f dB", m_Volume / 100.0f);
        break;
      }
      else
      {
        m->respond_double(m_player_audio->GetVolume());
        break;
      }
    }

  case ACTION_MUTE:
    if(m_player_audio) m_player_audio->SetMute(true);
    break;

  case ACTION_UNMUTE:
    if(m_player_audio) m_player_audio->SetMute(false);
    break;

  case GET_POSITION:
    // Returns the current position in microseconds
    m->respond_int64(m_av_clock->GetMediaTime());
    break;

  case GET_ASPECT:
    // Returns aspect ratio
    m->respond_double(m_omx_reader->GetAspectRatio());
    break;

  case GET_VIDEO_STREAM_COUNT:
    // Returns number of video streams
    m->respond_int64(m_omx_reader->VideoStreamCount());
    break;

  case GET_RES_WIDTH:
    // Returns width of video
    m->respond_int64(m_omx_reader->GetWidth());
    break;

  case GET_RES_HEIGHT:
    // Returns height of video
    m->respond_int64(m_omx_reader->GetHeight());
    break;

  case GET_DURATION:
    // Returns the duration in microseconds
    m->respond_int64(m_omx_reader->GetStreamLengthMicro());
    break;

  case SET_POSITION:
    // set position expects an additional argument over ACTION_SEEK_RELATIVE
    if(!m->ignore_arg())
    {
      m->respond_invalid_args();
      break;
    }
    // fall through
  case ACTION_SEEK_RELATIVE:
    {
      int64_t seek_pts;

      // Make sure a value is sent for setting position
      if(!m->get_arg_int64(&seek_pts))
      {
        m->respond_invalid_args();
        break;
      }

      int64_t cur_pts = m_av_clock->GetMediaTime();

      // make relative value absolute
      if(search_key == ACTION_SEEK_RELATIVE)
        seek_pts += cur_pts;

	  if(m_DvdPlayer)
	  {
	    int64_t byte_pos;
	    int cur_ch = m_DvdPlayer->getChapter(cur_pts);
	    int seek_ch = m_DvdPlayer->GetChapterInfo(seek_pts, byte_pos);

        if(cur_ch == seek_ch)
          break;

        if(!m_omx_reader->SeekBytes(byte_pos, seek_ch < cur_ch))
          break;

        show_progress_message("Chapter", seek_ch + 1);
        cur_pts = seek_pts;
      }
      else
      {
        if(!m_omx_reader->SeekTime(seek_pts, &cur_pts))
          break;

        show_progress_message("Seek", (int)(cur_pts * 1e-6));
      }

      FlushStreams(cur_pts);
      CLogLog(LOGDEBUG, "Seeked %lld", cur_pts);
      m->respond_int64(cur_pts);

      break;
    }

  case SET_ALPHA:
    {
      int64_t alpha;

      // Make sure a value is sent for setting alpha
      if(m->ignore_arg() && m->get_arg_int64(&alpha))
      {
        m->respond_invalid_args();
        break;
      }

      m->respond_int64(alpha);
      if(m_player_video) m_player_video->SetAlpha(alpha);
      break;
    }

  case SET_LAYER:
    {
      int64_t layer;

      // Make sure a value is sent for setting layer
      if(m->ignore_arg() && m->get_arg_int64(&layer))
      {
        m->respond_int64(layer);
        if(m_player_video) m_player_video->SetLayer(layer);
      }
      else
      {
        m->respond_invalid_args();
      }
      break;
    }

  case SET_ASPECT_MODE:
    {
      if(!m_player_video)
        break;

      const char *aspectMode;
      if(!m->ignore_arg() && m->get_arg_string(&aspectMode))
      {
        m->respond_invalid_args();
        break;
      }

      if (strcmp(aspectMode, "letterbox") == 0)
        m_config_video.aspectMode = 1;
      else if (strcmp(aspectMode, "fill") == 0)
        m_config_video.aspectMode = 2;
      else if (strcmp(aspectMode, "stretch") == 0)
        m_config_video.aspectMode = 3;
      else
      {
        m->respond_invalid_args();
        break;
      }

      m->respond_string(aspectMode);
      m_player_video->SetVideoRect(m_config_video.aspectMode);

      break;
    }

  case LIST_SUBTITLES:
    {
      std::vector<std::string> sub_list;
      m_omx_reader->GetMetaData(OMXSTREAM_SUBTITLE, sub_list);

      if(!m_external_subtitles_path.empty())
        sub_list.push_back(std::to_string(sub_list.size()) + ":und:external:srt:");

      // mark one as active
      int active_stream = m_player_subtitles->GetActiveStream();
      if(active_stream > -1)
        sub_list[active_stream] += "active";

      m->respond_array(sub_list);
      break;
    }

  case LIST_AUDIO:
    {
      std::vector<std::string> audio_list;
      m_omx_reader->GetMetaData(OMXSTREAM_AUDIO, audio_list);

      if(m_player_audio && !m_player_audio->GetMute())
        audio_list[m_player_audio->GetActiveStream()] += "active";

      m->respond_array(audio_list);
      break;
    }

  case LIST_VIDEO:
    {
      std::vector<std::string> video_list;
      m_omx_reader->GetMetaData(OMXSTREAM_VIDEO, video_list);

      if(m_player_video)
        video_list[0] += "active";

      m->respond_array(video_list);
      break;
    }

  case DO_ACTION:
    {
      int action;
      if(!m->get_arg_int(&action) || action >= START_OF_DBUS_METHODS)
      {
        m->respond_invalid_args();
        break;
      }

      return handle_event((Action)action, m);
    }

  case GET_MINIMUM_RATE:
    m->respond_double(playspeeds[1]);
    break;

  case GET_MAXIMUM_RATE:
    m->respond_double(playspeeds[playspeed_max]);
    break;

  case GET_RATE:
    //return current playing rate
    m->respond_double((double)m_av_clock->PlaySpeed()/1000.0f);
    break;

  case GET_VOLUME:
    //return current volume
    m->respond_double(m_player_audio ? m_player_audio->GetVolume() : 0.0f);
    break;

  case GET_METADATA:
    {
      std::string url = m_filename;
      if(!IsURL(url))
        url = "file://" + m_filename;

      int64_t duration = m_omx_reader->GetStreamLengthMicro();

      m->send_metadata(url.c_str(), &duration);
      break;
    }

  default:
    break;
  }

  return CONTINUE;
}


// we jump here when playing the next track in a dvd
int run_play_loop()
{
  try {
    m_omx_reader = new OMXReader(m_filename, m_dump_format, m_config_audio.is_live, m_DvdPlayer);
  }
  catch(const char *msg)
  {
    osd_printf(UM_ALL, "OMXReader error: %s", msg);
    m_omx_reader = NULL;
    return END_PLAY_WITH_ERROR;
  }

  if (m_dump_format_exit)
    return ABORT_PLAY;

  // what do we have
  m_has_subtitle  = !m_external_subtitles_path.empty() || m_omx_reader->SubtitleStreamCount() > 0;
  m_loop          = m_loop && m_omx_reader->CanSeek();

  // stop the clock
  m_av_clock->StateIdle();
  m_av_clock->Stop();
  m_av_clock->Pause();

  // display some startup osd
  if(m_DvdPlayer)
  {
    int ch;

    // seek at start
    if(m_incr > 0)
    {
      int64_t seek_bytes;
      int64_t seek_ts = (int64_t)m_incr * AV_TIME_BASE;
      ch = m_DvdPlayer->GetChapterInfo(seek_ts, seek_bytes);

      if(!m_omx_reader->SeekBytes(seek_bytes, false))
        m_incr = 0;
    }

    printf("Playing: %s, Track: %d\n", m_filename.c_str(), m_track + 1);

    if(m_incr > 0)
    {
      osd_printf(UM_LONG, "%s\nTrack %d\nChapter %d", m_DvdPlayer->GetTitle().c_str(),
        m_track + 1, ch + 1);
      m_incr = 0;
    }
    else
    {
      osd_printf(UM_LONG, "%s\nTrack %d", m_DvdPlayer->GetTitle().c_str(), m_track + 1);
    }

    UpdateRaspicastMetaData(m_DvdPlayer->GetTitle() + " - Track " + std::to_string(m_track + 1));
  }
  else
  {
    // seek at start
    if(m_incr > 0 && !m_omx_reader->SeekTime((int64_t)m_incr * AV_TIME_BASE, NULL, false))
      m_incr = 0;

    printf("Playing: %s\n", m_filename.c_str());
    std::string short_filename = getShortFileName();

    if(m_incr > 0)
    {
      int dur = m_omx_reader->GetStreamLengthSeconds();
      osd_printf(UM_LONG, "%s\n%02d:%02d:%02d / %02d:%02d:%02d", short_filename.c_str(),
        (m_incr/3600), (m_incr/60)%60, m_incr%60, (dur/3600), (dur/60)%60, dur%60);
      m_incr = 0;
    }
    else
    {
      osd_print(UM_LONG, short_filename.c_str());
    }
    UpdateRaspicastMetaData(short_filename);
  }

  /* -------------------------------------------------------
                           Video Setup
     ------------------------------------------------------- */

  if(m_omx_reader->VideoStreamCount() > 0)
  {
    m_config_video.hints = m_omx_reader->GetHints(OMXSTREAM_VIDEO, 0);

    if(m_fps > 0.0f)
    {
      m_config_video.hints.fpsrate = m_fps * AV_TIME_BASE;
      m_config_video.hints.fpsscale = AV_TIME_BASE;
    }

    if(m_refresh)
    {
      m_video_core.saveTVState();
      m_video_core.SetVideoMode(&m_config_video.hints, m_3d, m_NativeDeinterlace);
    }

    // get display aspect
    m_config_video.display_aspect = m_video_core.getDisplayAspect();

    if(m_orientation >= 0)
      m_config_video.hints.orientation = m_orientation;

    try {
      m_player_video = new OMXPlayerVideo(m_av_clock, m_config_video);
    }
    catch(const char *msg)
    {
      puts(msg);
      m_player_video = NULL;
      return END_PLAY_WITH_ERROR;
    }
  }

  /* -------------------------------------------------------
                           Audio Setup
     ------------------------------------------------------- */

  if(m_audio_index != -2 && m_omx_reader->AudioStreamCount() > 0)
  {
    // validate command line provided info
    if(m_audio_index >= m_omx_reader->AudioStreamCount())
    {
      printf("Error: file has only %d audio streams\n", m_omx_reader->AudioStreamCount());
      return END_PLAY_WITH_ERROR;
    }

    // an audio string overrides any provided stream number
    if(m_audio_lang[0] != '\0')
      m_audio_index = m_omx_reader->GetStreamByLanguage(OMXSTREAM_AUDIO, m_audio_lang);

    // select an audio stream to play when not already selected
    // Where no audio stream has been selected, use the first stream other than audio narrative
    if(m_audio_index == -1)
    {
      for(int i = 0; i < m_omx_reader->AudioStreamCount(); i++)
      {
        if(m_omx_reader->GetStreamLanguage(OMXSTREAM_AUDIO, i) != "NAR")
        {
          m_audio_index = i;
          break;
        }
      }
      if(m_audio_index == -1)
        m_audio_index = 0;
    }
    printf("Selecting audio stream: %d\n", m_audio_index + 1);

    // get audio hints (ie params, info) from OMXReader
    m_config_audio.hints = m_omx_reader->GetHints(OMXSTREAM_AUDIO, m_audio_index);

    if(m_config_audio.hints.codec == AV_CODEC_ID_AC3 || m_config_audio.hints.codec == AV_CODEC_ID_EAC3)
    {
      if(m_video_core.canPassThroughAC3())
        m_config_audio.passthrough = false;
    }
    else if(m_config_audio.hints.codec == AV_CODEC_ID_DTS)
    {
      if(m_video_core.canPassThroughDTS())
        m_config_audio.passthrough = false;
    }

    // start audio decoder encoder
    try {
      m_player_audio = new OMXPlayerAudio(m_av_clock, m_config_audio, m_omx_reader, m_audio_index);
    }
    catch(const char *msg)
    {
      osd_printf(UM_ALL, msg);
      m_player_audio = NULL;
      return END_PLAY_WITH_ERROR;
    }

    // set volume
    m_player_audio->SetVolume(pow(10, m_Volume / 2000.0));
    if (m_Amplification)
      m_player_audio->SetDynamicRangeCompression(m_Amplification);
  }

  /* -------------------------------------------------------
                         Subtitle Setup
     ------------------------------------------------------- */

  if(m_has_subtitle)
  {
    if(!m_player_subtitles->Open(m_omx_reader->SubtitleStreamCount(), m_external_subtitles_path))
    {
      osd_printf(UM_ALL, "Failed to open subtitles");
      return END_PLAY_WITH_ERROR;
    }

    if(m_cmd_line_subtitles)
      m_subtitle_index = m_omx_reader->SubtitleStreamCount() - 1;
    else if(m_subtitle_lang[0] != '\0')
      m_subtitle_index = m_omx_reader->GetStreamByLanguage(OMXSTREAM_SUBTITLE, m_subtitle_lang);

    m_player_subtitles->SetActiveStream(m_subtitle_index);

    // Check if we have any DVD subtitles (these can be on ordinary media files as well as DVDs)
    // If so, setup a dispmanx layer to display them
    Dimension sub_dim(m_config_video.hints.width, m_config_video.hints.height);
    float sub_aspect = m_config_video.hints.aspect;
    uint32_t *palette = m_DvdPlayer ? m_DvdPlayer->getPalette() : NULL;

    if(m_omx_reader->FindDVDSubs(sub_dim, sub_aspect, &palette))
    {
      Rect view_port = DispmanxLayer::GetVideoPort(sub_aspect, m_config_video.aspectMode);

      if(!m_player_subtitles->initDVDSubs(view_port, sub_dim, palette))
      {
        osd_printf(UM_ALL, "Failed to initialise DVD subtitles");
        return END_PLAY_WITH_ERROR;
      }
    }

    if(!m_DvdPlayer && palette != NULL)
      delete palette;
  }

  PrintSubtitleInfo();

  if(m_audio_index != -2)
    m_audio_index = -1;

  // start the clock
  m_av_clock->Reset(m_player_video, m_player_audio);
  m_av_clock->StateExecute();

  // forget seek time of all files being played
  if(!m_is_dvd_device) m_file_store.forget(m_filename);

  int64_t last_check_time = 0;

  while(!m_stopped)
  {
    int64_t now = OMXClock::GetAbsoluteClock();
    bool update = false;

    if (last_check_time == 0 || last_check_time + 20000 <= now)
    {
      update = true;
      last_check_time = now;
    }

    if (update) {
      enum Action action;
      enum ControlFlow next;

      if(!m_keyboard || (action = m_keyboard->getEvent()) == INVALID_ACTION)
        action = m_cec_listener.getEvent();

      if(action != INVALID_ACTION)
        next = handle_event(action, NULL);
      else if(m_omxcontrol)
        next = m_omxcontrol.getEvent();
      else
        next = CONTINUE;

      if(next != CONTINUE)
        return next;
    }

    /* player got in an error state */
    if(m_player_audio && m_player_audio->Error())
    {
      osd_printf(UM_ALL, "Audio player error");
      return END_PLAY_WITH_ERROR;
    }

    if (update)
    {
      /* when the video/audio fifos are low, we pause clock, when high we resume */
      int64_t stamp = m_av_clock->GetMediaTime();
      int64_t audio_pts = m_player_audio ? m_player_audio->GetCurrentPTS() : AV_NOPTS_VALUE;
      int64_t video_pts = m_player_video ? m_player_video->GetCurrentPTS() : AV_NOPTS_VALUE;

      float audio_fifo = audio_pts == AV_NOPTS_VALUE ? 0.0f : (audio_pts - stamp) * 1e-6;
      float video_fifo = video_pts == AV_NOPTS_VALUE ? 0.0f : (video_pts - stamp) * 1e-6;
      float threshold = 0.0;
      if(m_player_audio)
        threshold = std::min(0.1f, (float)m_player_audio->GetCacheTotal() * 0.1f);
      else if(m_player_video)
        threshold = std::min(0.1f, (float)m_player_video->GetDecoderBufferSize() * 0.1f);

      bool audio_fifo_low = false, video_fifo_low = false, audio_fifo_high = false, video_fifo_high = false;

      if(m_stats)
      {
        static int count;
        if ((count++ & 7) == 0)
        {
          if(m_player_video && m_player_audio)
            printf("M:%lld V:%6.2fs %6dk/%6dk A:%6.2f %llds/%llds Cv:%6uk Ca:%6uk                            \r", stamp,
                 video_fifo, (m_player_video->GetDecoderBufferSize()-m_player_video->GetDecoderFreeSpace())>>10, m_player_video->GetDecoderBufferSize()>>10,
                 audio_fifo, m_player_audio->GetDelay(), m_player_audio->GetCacheTotal(),
                 m_player_video->GetCached()>>10, m_player_audio->GetCached()>>10);
          else if(m_player_video)
            printf("M:%lld V:%6.2fs %6dk/%6dk A:  0.00 0s/0s Cv:%6uk Ca:     0k                            \r", stamp,
                 video_fifo, (m_player_video->GetDecoderBufferSize()-m_player_video->GetDecoderFreeSpace())>>10, m_player_video->GetDecoderBufferSize()>>10,
                 m_player_video->GetCached()>>10);
          else if(m_player_audio)
            printf("M:%lld V:  0.00s      0k/     0k A:%6.2f %llds/%llds Cv:     0k Ca:%6uk                            \r", stamp,
                 audio_fifo, m_player_audio->GetDelay(), m_player_audio->GetCacheTotal(),
                 m_player_audio->GetCached()>>10);
        }
      }

      if (audio_pts != AV_NOPTS_VALUE)
      {
        audio_fifo_low = m_player_audio && audio_fifo < threshold;
        audio_fifo_high = !m_player_audio || audio_fifo > m_threshold;
      }
      if (video_pts != AV_NOPTS_VALUE)
      {
        video_fifo_low = m_player_video && video_fifo < threshold;
        video_fifo_high = !m_player_video || video_fifo > m_threshold;
      }

      // keep latency under control by adjusting clock (and so resampling audio)
      if (m_config_audio.is_live)
      {
        float latency = AV_NOPTS_VALUE;
        if (m_player_audio && audio_pts != AV_NOPTS_VALUE)
          latency = audio_fifo;
        else if (!m_player_audio && m_player_video && video_pts != AV_NOPTS_VALUE)
          latency = video_fifo;
        if (!m_Pause && latency != AV_NOPTS_VALUE)
        {
          if (m_av_clock->IsPaused())
          {
            if (latency > m_threshold)
            {
              CLogLog(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_omx_reader->IsEof(), m_omx_pkt);
              m_av_clock->Resume();
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

            m_av_clock->SetSpeed(speed);
            CLogLog(LOGDEBUG, "Live: %.2f (%.2f) S:%.3f T:%.2f", m_latency, latency, speed, m_threshold);
          }
        }
      }
      else if(!m_Pause && (m_omx_reader->IsEof() || m_omx_pkt || (audio_fifo_high && video_fifo_high)))
      {
        if (m_av_clock->IsPaused())
        {
          CLogLog(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_omx_reader->IsEof(), m_omx_pkt);
          m_av_clock->Resume();
        }
      }
      else if (m_Pause || audio_fifo_low || video_fifo_low)
      {
        if (!m_av_clock->IsPaused())
        {
          if (!m_Pause)
            m_threshold = std::min(2.0f*m_threshold, 16.0f);
          CLogLog(LOGDEBUG, "Pause %.2f,%.2f (%d,%d,%d,%d) %.2f", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_threshold);
          m_av_clock->Pause();
        }
      }
    }

    if(!m_omx_pkt)
      m_omx_pkt = m_omx_reader->Read();

    if(m_omx_pkt)
      m_send_eos = false;

    if(m_omx_reader->IsEof() && !m_omx_pkt)
    {
      if (!m_send_eos && m_player_video)
        m_player_video->SubmitEOS();
      if (!m_send_eos && m_player_audio)
        m_player_audio->SubmitEOS();
      m_send_eos = true;
      if ( (m_player_video && !m_player_video->IsEOS()) ||
           (m_player_audio && !m_player_audio->IsEOS()) )
      {
        OMXClock::Sleep(10);
        continue;
      }

      if (m_loop)
      {
        int64_t seek_ts = (int64_t)m_loop_from * AV_TIME_BASE;
        if(m_DvdPlayer)
        {
          int64_t seek_bytes;
          m_DvdPlayer->GetChapterInfo(seek_ts, seek_bytes);
          if(m_omx_reader->SeekBytes(seek_bytes, true))
          {
            FlushStreams(seek_ts);
            continue;
          }
        }
        else
        {
          if(m_omx_reader->SeekTime(seek_ts, NULL, true))
          {
            FlushStreams(seek_ts);
            continue;
          }
        }
      }

      break;
    }

    if(!m_omx_pkt)
    {
      OMXClock::Sleep(10);
      continue;
    }

    switch(m_omx_pkt->codec_type)
    {
    case AVMEDIA_TYPE_VIDEO:
      if(!m_player_video || m_omx_pkt->index != 0)
        goto discard_packet;

      if(m_player_video->AddPacket(m_omx_pkt))
        m_omx_pkt = NULL;
      else
        OMXClock::Sleep(10);
      break;

    case AVMEDIA_TYPE_AUDIO:
      if(!m_player_audio || playspeed_current != playspeed_normal)
        goto discard_packet;

      if(m_player_audio->AddPacket(m_omx_pkt))
        m_omx_pkt = NULL;
      else
        OMXClock::Sleep(10);
      break;

    case AVMEDIA_TYPE_SUBTITLE:
      if(m_has_subtitle && playspeed_current == playspeed_normal)
        m_player_subtitles->AddPacket(m_omx_pkt);
      m_omx_pkt = NULL;
      break;

    discard_packet:
    default:
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

  // close first
  m_player_subtitles->Close();
  m_cmd_line_subtitles = false;

  int t = (int)(m_av_clock->GetMediaTime()*1e-6);
  int dur = m_omx_reader ? m_omx_reader->GetStreamLengthSeconds() : 0;
  printf("Stopped at: %02d:%02d:%02d\n", (t/3600), (t/60)%60, t%60);
  printf("  Duration: %02d:%02d:%02d\n", (dur/3600), (dur/60)%60, dur%60);

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

  // flush streams
  FlushStreams();

  safe_delete(m_player_video);
  safe_delete(m_player_audio);
  safe_delete(m_omx_reader);

  // stop seeking
  m_incr = 0;
}


int playlist_control()
{
  int t = (int)(m_av_clock->GetMediaTime()*1e-6);

  if(m_playlist_enabled) {
    if(!m_stopped && m_send_eos && m_next_prev_file == 0)
      m_next_prev_file = 1;
    
    if(m_next_prev_file != 0) {
      // if this is a DVD look for next track
      if(m_DvdPlayer) {
        if(m_DvdPlayer->ChangeTrack(m_next_prev_file, m_track))
        {
          m_firstfile = false;
          m_next_prev_file = 0;
          m_subtitle_index = -1;
          m_audio_index = -1;
          return RUN_PLAY_LOOP;
        }

        // no more tracks to play, exit DVD mode
        delete m_DvdPlayer;
        m_DvdPlayer = NULL;
      }

      // Play next file in playlist if there is one...
      // 'Exists' checks if file is readable
      if(!m_is_dvd_device && m_playlist.ChangeFile(m_next_prev_file, m_filename)
          && Exists(m_filename)) {
        m_firstfile = false;
        m_next_prev_file = 0;
        m_subtitle_index = -1;
        m_audio_index = -1;
        return CHANGE_PLAYLIST_ITEM;
      }
    } else if(!m_firstfile || t > 5) {
      if(m_is_dvd_device)
        m_dvd_store.remember(m_track, t, &m_audio_lang[0], &m_subtitle_lang[0]);
      else
        m_file_store.remember(m_filename, m_track, t,
                              &m_audio_lang[0], m_audio_index,
                              &m_subtitle_lang[0], m_subtitle_index);
    }
  }

  if(!m_replacement_filename.empty()) {
    // we've received a new file to play via dbus
    safe_delete(m_DvdPlayer);

    if(m_playlist_enabled) {
      if(m_is_dvd_device) m_dvd_store.saveStore();
      else m_file_store.saveStore();
    }

    m_filename = m_replacement_filename;
    m_replacement_filename.clear();

    m_cmd_line_subtitles = false;
    m_external_subtitles_path.clear();

    m_firstfile = true;
    m_subtitle_index = -1;
    m_audio_index = -1;

    return CHANGE_FILE;
  }

  return SHUTDOWN;
}

int shutdown(bool exit_with_error)
{
  // We may get here after receiving an error
  // so be conservative and check before deleting objects
  safe_delete(m_omx_pkt);
  safe_delete(m_player_video);
  safe_delete(m_player_audio);
  safe_delete(m_DvdPlayer);

  // Exit on failure
  if(exit_with_error)
    return EXIT_FAILURE;

  // save recent files
  if(m_playlist_enabled) {
    if(m_is_dvd_device) m_dvd_store.saveStore();
    else m_file_store.saveStore();
  }

  puts("have a nice day ;)");

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

int main(int argc, char *argv[])
try {
  // control loop
  int rv = startup(argc, argv);

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
catch(const char *msg)
{
  puts(msg);
  return EXIT_FAILURE;
}
