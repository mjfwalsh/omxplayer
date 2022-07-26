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

  float getDisplayAspect();
  const char *getAudioDevice();
  bool canPassThroughAC3();
  bool canPassThroughDTS();
  void turnOffNativeDeinterlace();
}
