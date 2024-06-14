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

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <string>

#include "PCMRemap.h"
#include "utils/log.h"

static enum PCMChannels PCMLayoutMap[PCM_MAX_LAYOUT][PCM_MAX_CH + 1] =
{
  /* 2.0 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_INVALID},
  /* 2.1 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_LOW_FREQUENCY, PCM_INVALID},
  /* 3.0 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_FRONT_CENTER, PCM_INVALID},
  /* 3.1 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_FRONT_CENTER, PCM_LOW_FREQUENCY, PCM_INVALID},
  /* 4.0 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_BACK_LEFT, PCM_BACK_RIGHT, PCM_INVALID},
  /* 4.1 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_BACK_LEFT, PCM_BACK_RIGHT, PCM_LOW_FREQUENCY, PCM_INVALID},
  /* 5.0 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_FRONT_CENTER, PCM_BACK_LEFT, PCM_BACK_RIGHT, PCM_INVALID},
  /* 5.1 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_FRONT_CENTER, PCM_BACK_LEFT, PCM_BACK_RIGHT, PCM_LOW_FREQUENCY, PCM_INVALID},
  /* 7.0 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_FRONT_CENTER, PCM_SIDE_LEFT, PCM_SIDE_RIGHT, PCM_BACK_LEFT, PCM_BACK_RIGHT, PCM_INVALID},
  /* 7.1 */ {PCM_FRONT_LEFT, PCM_FRONT_RIGHT, PCM_FRONT_CENTER, PCM_SIDE_LEFT, PCM_SIDE_RIGHT, PCM_BACK_LEFT, PCM_BACK_RIGHT, PCM_LOW_FREQUENCY, PCM_INVALID}
};

/*
  map missing output into channel @ volume level
  the order of this table is important, mix tables can not depend on channels that have not been defined yet
  eg, FC can only be mixed into FL, FR as they are the only channels that have been defined
*/
#define PCM_MAX_MIX 3
static struct PCMMapInfo PCMDownmixTable[PCM_MAX_CH][PCM_MAX_MIX] =
{
  /* PCM_FRONT_LEFT */
  {
    {PCM_INVALID}
  },
  /* PCM_FRONT_RIGHT */
  {
    {PCM_INVALID}
  },
  /* PCM_FRONT_CENTER */
  {
    {PCM_FRONT_LEFT_OF_CENTER , 1.0},
    {PCM_FRONT_RIGHT_OF_CENTER, 1.0},
    {PCM_INVALID}
  },
  /* PCM_LOW_FREQUENCY */
  {
    /*
      A/52B 7.8 paragraph 2 recomends +10db
      but due to horrible clipping when normalize
      is disabled we set this to 1.0
    */
    {PCM_FRONT_LEFT           , 1.0},//3.5},
    {PCM_FRONT_RIGHT          , 1.0},//3.5},
    {PCM_INVALID}
  },
  /* PCM_BACK_LEFT */
  {
    {PCM_FRONT_LEFT           , 1.0},
    {PCM_INVALID}
  },
  /* PCM_BACK_RIGHT */
  {
    {PCM_FRONT_RIGHT          , 1.0},
    {PCM_INVALID}
  },
  /* PCM_FRONT_LEFT_OF_CENTER */
  {
    {PCM_FRONT_LEFT           , 1.0},
    {PCM_FRONT_CENTER         , 1.0, true},
    {PCM_INVALID}
  },
  /* PCM_FRONT_RIGHT_OF_CENTER */
  {
    {PCM_FRONT_RIGHT          , 1.0},
    {PCM_FRONT_CENTER         , 1.0, true},
    {PCM_INVALID}
  },
  /* PCM_BACK_CENTER */
  {
    {PCM_BACK_LEFT            , 1.0},
    {PCM_BACK_RIGHT           , 1.0},
    {PCM_INVALID}
  },
  /* PCM_SIDE_LEFT */
  {
    {PCM_FRONT_LEFT           , 1.0},
    {PCM_BACK_LEFT            , 1.0},
    {PCM_INVALID}
  },
  /* PCM_SIDE_RIGHT */
  {
    {PCM_FRONT_RIGHT          , 1.0},
    {PCM_BACK_RIGHT           , 1.0},
    {PCM_INVALID}
  },
  /* PCM_TOP_FRONT_LEFT */
  {
    {PCM_FRONT_LEFT           , 1.0},
    {PCM_INVALID}
  },
  /* PCM_TOP_FRONT_RIGHT */
  {
    {PCM_FRONT_RIGHT          , 1.0},
    {PCM_INVALID}
  },
  /* PCM_TOP_FRONT_CENTER */
  {
    {PCM_TOP_FRONT_LEFT       , 1.0},
    {PCM_TOP_FRONT_RIGHT      , 1.0},
    {PCM_INVALID}
  },
  /* PCM_TOP_CENTER */
  {
    {PCM_TOP_FRONT_LEFT       , 1.0},
    {PCM_TOP_FRONT_RIGHT      , 1.0},
    {PCM_INVALID}
  },
  /* PCM_TOP_BACK_LEFT */
  {
    {PCM_BACK_LEFT            , 1.0},
    {PCM_INVALID}
  },
  /* PCM_TOP_BACK_RIGHT */
  {
    {PCM_BACK_RIGHT           , 1.0},
    {PCM_INVALID}
  },
  /* PCM_TOP_BACK_CENTER */
  {
    {PCM_TOP_BACK_LEFT        , 1.0},
    {PCM_TOP_BACK_RIGHT       , 1.0},
    {PCM_INVALID}
  }
};

/* resolves the channels recursively and returns the new index of tablePtr */
struct PCMMapInfo* CPCMRemap::ResolveChannel(enum PCMChannels channel, float level, bool ifExists, std::vector<enum PCMChannels> path, struct PCMMapInfo *tablePtr)
{
  if (channel == PCM_INVALID) return tablePtr;

  /* if its a 1 to 1 mapping, return */
  if (m_useable[channel])
  {
    tablePtr->channel = channel;
    tablePtr->level   = level;

    ++tablePtr;
    tablePtr->channel = PCM_INVALID;
    return tablePtr;
  } else
    if (ifExists)
      level /= 2;

  struct PCMMapInfo *info;
  std::vector<enum PCMChannels>::iterator itt;

  for(info = PCMDownmixTable[channel]; info->channel != PCM_INVALID; ++info)
  {
    /* make sure we are not about to recurse into ourself */
    bool found = false;
    for(itt = path.begin(); itt != path.end(); ++itt)
      if (*itt == info->channel)
      {
        found = true;
        break;
      }

    if (found)
      continue;

    path.push_back(channel);
    float  l = (info->level * (level / 100)) * 100;
    tablePtr = ResolveChannel(info->channel, l, info->ifExists, path, tablePtr);
    path.pop_back();
  }

  return tablePtr;
}

/*
  Builds a lookup table without extra adjustments, useful if we simply
  want to find out which channels are active.
  For final adjustments, BuildMap() is used.
*/
void CPCMRemap::ResolveChannels()
{
  bool hasSide = false;
  bool hasBack = false;

  memset(m_useable, 0, sizeof(m_useable));

  /* figure out what channels we have and can use */
  for(enum PCMChannels *chan = PCMLayoutMap[m_channelLayout]; *chan != PCM_INVALID; ++chan)
  {
    for(unsigned int out_ch = 0; out_ch < m_outChannels; ++out_ch)
      if (m_outMap[out_ch] == *chan)
      {
        m_useable[*chan] = true;
        break;
      }
  }

  /* force mono audio to front left and front right */
  if (m_inChannels == 1 && m_inMap[0] == PCM_FRONT_CENTER
      && m_useable[PCM_FRONT_LEFT] && m_useable[PCM_FRONT_RIGHT])
  {
    CLogLog(LOGDEBUG, "CPCMRemap: Mapping mono audio to front left and front right");
    m_useable[PCM_FRONT_CENTER] = false;
    m_useable[PCM_FRONT_LEFT_OF_CENTER] = false;
    m_useable[PCM_FRONT_RIGHT_OF_CENTER] = false;
  }

  /* see if our input has side/back channels */
  for(unsigned int in_ch = 0; in_ch < m_inChannels; ++in_ch)
    switch(m_inMap[in_ch])
    {
      case PCM_SIDE_LEFT:
      case PCM_SIDE_RIGHT:
        hasSide = true;
        break;

      case PCM_BACK_LEFT:
      case PCM_BACK_RIGHT:
        hasBack = true;
        break;

      default:;
    }

  /* if our input has side, and not back channels, and our output doesnt have side channels */
  if (hasSide && !hasBack && (!m_useable[PCM_SIDE_LEFT] || !m_useable[PCM_SIDE_RIGHT]))
  {
    CLogLog(LOGDEBUG, "CPCMRemap: Forcing side channel map to back channels");
    for(unsigned int in_ch = 0; in_ch < m_inChannels; ++in_ch)
           if (m_inMap[in_ch] == PCM_SIDE_LEFT ) m_inMap[in_ch] = PCM_BACK_LEFT;
      else if (m_inMap[in_ch] == PCM_SIDE_RIGHT) m_inMap[in_ch] = PCM_BACK_RIGHT;
  }

  /* resolve all the channels */
  struct PCMMapInfo table[PCM_MAX_CH + 1], *info, *dst;
  std::vector<enum PCMChannels> path;

  for (int i = 0; i < PCM_MAX_CH + 1; i++)
  {
    for (int j = 0; j < PCM_MAX_CH + 1; j++)
      m_lookupMap[i][j].channel = PCM_INVALID;
  }

  memset(m_counts, 0, sizeof(m_counts));
  for(unsigned int in_ch = 0; in_ch < m_inChannels; ++in_ch)
  {

    for (int i = 0; i < PCM_MAX_CH + 1; i++)
      table[i].channel = PCM_INVALID;

    ResolveChannel(m_inMap[in_ch], 1.0f, false, path, table);
    for(info = table; info->channel != PCM_INVALID; ++info)
    {
      /* find the end of the table */
      for(dst = m_lookupMap[info->channel]; dst->channel != PCM_INVALID; ++dst);

      /* append it to the table and set its input offset */
      dst->channel   = m_inMap[in_ch];
      dst->in_offset = in_ch * 2;
      dst->level     = info->level;
      m_counts[dst->channel]++;
    }
  }
}

/*
  builds a lookup table to convert from the input mapping to the output
  mapping, this decreases the amount of work per sample to remap it.
*/
void CPCMRemap::BuildMap()
{
  struct PCMMapInfo *dst;

  /* see if we need to normalize the levels */
  CLogLog(LOGDEBUG, "CPCMRemap: Downmix normalization is %s", (m_dontnormalize ? "disabled" : "enabled"));

  /* convert the levels into RMS values */
  float loudest    = 0.0;
  bool  hasLoudest = false;

  for(unsigned int out_ch = 0; out_ch < m_outChannels; ++out_ch)
  {
    float scale = 0;
    int count = 0;
    for(dst = m_lookupMap[m_outMap[out_ch]]; dst->channel != PCM_INVALID; ++dst)
    {
      dst->copy  = false;
      dst->level = dst->level / sqrt((float)m_counts[dst->channel]);
      scale     += dst->level;
      ++count;
    }

    /* if there is only 1 channel to mix, and the level is 1.0, then just copy the channel */
    dst = m_lookupMap[m_outMap[out_ch]];
    if (count == 1 && dst->level > 0.99 && dst->level < 1.01)
      dst->copy = true;

    /* normalize the levels if it is turned on */
    if (!m_dontnormalize)
      for(dst = m_lookupMap[m_outMap[out_ch]]; dst->channel != PCM_INVALID; ++dst)
      {
        dst->level /= scale;
        /* find the loudest output level we have that is not 1-1 */
        if (dst->level < 1.0 && loudest < dst->level)
        {
          loudest    = dst->level;
          hasLoudest = true;
        }
      }
  }

  /* adjust the channels that are too loud */
  for(unsigned int out_ch = 0; out_ch < m_outChannels; ++out_ch)
  {
    std::string s;
    for(dst = m_lookupMap[m_outMap[out_ch]]; dst->channel != PCM_INVALID; ++dst)
    {
      if (hasLoudest && dst->copy)
      {
        dst->level = loudest;
        dst->copy  = false;
      }

      if(g_logging_enabled)
      {
        s += PCMChannelStr(dst->channel);
        s += "(" + std::to_string(dst->level) + (dst->copy ? "*" : "") + ") ";
      }
    }
    CLogLog(LOGDEBUG, "CPCMRemap: %s = %s", PCMChannelStr(m_outMap[out_ch]), s.c_str());
  }
}

void CPCMRemap::DumpMap(const char *type, unsigned int channels, enum PCMChannels *channelMap)
{
  if(!g_logging_enabled) return;

  std::string mapping;
  for(unsigned int i = 0; i < channels; i++)
  {
    mapping += PCMChannelStr(channelMap[i]);
    mapping += ",";
  }
  mapping.pop_back();

  _CLogLog(LOGINFO, "CPCMRemap: %s channel map: %s", type, mapping.c_str());
}

/* sets the input format, and returns the requested channel layout */
CPCMRemap::CPCMRemap(unsigned int inChannels, enum PCMChannels *inChannelMap, unsigned int outChannels, enum PCMChannels *outChannelMap, enum PCMLayout channelLayout, bool dontnormalize)
:
  m_channelLayout(channelLayout),
  m_inChannels(inChannels),
  m_outChannels(outChannels),
  m_dontnormalize(dontnormalize)
{
  assert(inChannelMap != NULL);
  assert(outChannelMap != NULL);

  if (m_channelLayout >= PCM_MAX_LAYOUT) m_channelLayout = PCM_LAYOUT_2_0;

  memcpy(m_inMap, inChannelMap, sizeof(enum PCMChannels) * inChannels);
  memcpy(m_outMap, outChannelMap, sizeof(enum PCMChannels) * outChannels);

  DumpMap("I", inChannels, inChannelMap);
  DumpMap("O", outChannels, outChannelMap);

  ResolveChannels();

  BuildMap();
}

const char *CPCMRemap::PCMChannelStr(enum PCMChannels ename)
{
  switch(ename) {
    case 0:     return "FL";
    case 1:     return "FR";
    case 2:     return "CE";
    case 3:     return "LFE";
    case 4:     return "BL";
    case 5:     return "BR";
    case 6:     return "FLOC";
    case 7:     return "FROC";
    case 8:     return "BC";
    case 9:     return "SL";
    case 10:    return "SR";
    case 11:    return "TFL";
    case 12:    return "TFR";
    case 13:    return "TFC";
    case 14:    return "TC";
    case 15:    return "TBL";
    case 16:    return "TBR";
    case 17:    return "TBC";
    default:    return "???";
  }
}


void CPCMRemap::GetDownmixMatrix(float *downmix)
{
  for (int i=0; i<8*8; i++)
    downmix[i] = 0.0f;

  for (unsigned int ch = 0; ch < m_outChannels; ch++)
  {
    struct PCMMapInfo *info = m_lookupMap[m_outMap[ch]];
    if (info->channel == PCM_INVALID)
      continue;

    for(; info->channel != PCM_INVALID; info++)
      downmix[8*ch + (info->in_offset>>1)] = info->level;
  }
}
