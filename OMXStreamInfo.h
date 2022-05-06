#pragma once
/*
 *      Copyright (C) 2005-2008 Team XBMC
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

#include "DllAvCodec.h"

class COMXStreamInfo
{
public:
  enum AVCodecID codec = AV_CODEC_ID_NONE;

  // VIDEO
  int fpsscale = 0; // scale of 1000 and a rate of 29970 will result in 29.97 fps
  int fpsrate = 0;
  int height = 0; // height of the stream reported by the demuxer
  int width = 0; // width of the stream reported by the demuxer
  float aspect = 0.0f; // display aspect as reported by demuxer
  bool forced_aspect = false; // true if we trust container aspect more than codec
  int profile = 0; // encoder profile of the stream reported by the decoder. used to qualify hw decoders.
  int orientation = 0; // video orientation in clockwise degrees

  // AUDIO
  int channels = 0;
  int samplerate = 0;
  int bitrate = 0;
  int blockalign = 0;
  int bitspersample = 0;

  // CODEC EXTRADATA
  void*        extradata = NULL; // extra data for codec to use
  unsigned int extrasize = 0; // size of extra data
};
