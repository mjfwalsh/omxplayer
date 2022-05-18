#pragma once

typedef enum {
  CONF_FLAGS_FORMAT_NONE,
  CONF_FLAGS_FORMAT_SBS,
  CONF_FLAGS_FORMAT_TB,
  CONF_FLAGS_FORMAT_FP
} FORMAT_3D_T;

namespace VideoCore {
  void tv_stuff_init();

  int get_mem_gpu();
  void SetVideoMode(COMXStreamInfo *hints, FORMAT_3D_T is3d, bool NativeDeinterlace);
  bool blank_background(uint32_t rgba, int layer, int video_display);
  void saveTVState();
  void restoreTVState();

  float getDisplayAspect();
  std::string getAudioDevice();
  bool canPassThroughAC3();
  bool canPassThroughDTS();
  void turnOffNativeDeinterlace();
}