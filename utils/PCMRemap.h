#pragma once
/*
 *      Copyright (C) 2005-2010 Team XBMC
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

#include <vector>
#include <stdint.h>

#define PCM_MAX_CH 18
enum PCMChannels
{
  PCM_INVALID = -1,
  PCM_FRONT_LEFT,
  PCM_FRONT_RIGHT,
  PCM_FRONT_CENTER,
  PCM_LOW_FREQUENCY,
  PCM_BACK_LEFT,
  PCM_BACK_RIGHT,
  PCM_FRONT_LEFT_OF_CENTER,
  PCM_FRONT_RIGHT_OF_CENTER,
  PCM_BACK_CENTER,
  PCM_SIDE_LEFT,
  PCM_SIDE_RIGHT,
  PCM_TOP_FRONT_LEFT,
  PCM_TOP_FRONT_RIGHT,
  PCM_TOP_FRONT_CENTER,
  PCM_TOP_CENTER,
  PCM_TOP_BACK_LEFT,
  PCM_TOP_BACK_RIGHT,
  PCM_TOP_BACK_CENTER
};

#define PCM_MAX_LAYOUT 10
enum PCMLayout
{
  PCM_LAYOUT_2_0 = 0,
  PCM_LAYOUT_2_1,
  PCM_LAYOUT_3_0,
  PCM_LAYOUT_3_1,
  PCM_LAYOUT_4_0,
  PCM_LAYOUT_4_1,
  PCM_LAYOUT_5_0,
  PCM_LAYOUT_5_1,
  PCM_LAYOUT_7_0,
  PCM_LAYOUT_7_1
};

struct PCMMapInfo
{
  enum  PCMChannels channel;
  float level = 0.0f;
  bool  ifExists = false;
  int   in_offset = 0;
  bool  copy = false;
};

//!  Channels remapper class
/*!
   The usual set-up process:
   - user calls SetInputFormat with the input channels information
   - SetInputFormat responds with a channelmap corresponding to the speaker
     layout that the user has configured, with empty (according to information
     calculated from the input channelmap) channels removed
   - user uses this information to create the desired output channelmap,
     and calls SetOutputFormat to set it (if the channelmap contains channels
     that do not exist in the configured speaker layout, they will contain
     only silence)
 */

class CPCMRemap
{
protected:
  enum PCMLayout     m_channelLayout;
  int                m_inChannels, m_outChannels;
  enum PCMChannels   m_inMap [PCM_MAX_CH];
  enum PCMChannels   m_outMap[PCM_MAX_CH];

  bool               m_useable  [PCM_MAX_CH];
  struct PCMMapInfo  m_lookupMap[PCM_MAX_CH + 1][PCM_MAX_CH + 1];
  int                m_counts[PCM_MAX_CH];

  bool               m_dontnormalize;

  struct PCMMapInfo* ResolveChannel(enum PCMChannels channel, float level, bool ifExists, std::vector<enum PCMChannels> path, struct PCMMapInfo *tablePtr);
  void               ResolveChannels();
  void               BuildMap();
  void               DumpMap(const char *type, unsigned int channels, const enum PCMChannels *channelMap);
  const char*        PCMChannelStr(enum PCMChannels ename);
  const char*        PCMLayoutStr(enum PCMLayout ename);

public:
  CPCMRemap(int inChannels, const enum PCMChannels *inChannelMap, int outChannels, const enum PCMChannels *outChannelMap, enum PCMLayout channelLayout, bool dontnormalize);
  void GetDownmixMatrix(float *downmix);
  static int CountBits(int64_t value);
};
