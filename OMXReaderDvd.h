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

#ifndef _OMX_READER_DVD_H_
#define _OMX_READER_DVD_H_

#include <string>
#include <stdint.h>

#include "OMXReader.h"

class OMXDvdPlayer;

class OMXReaderDvd : public OMXReader
{
public:
  OMXReaderDvd(std::string &filename, bool dump_format, OMXDvdPlayer *dvd);
  ~OMXReaderDvd();

  SeekResult SeekChapter(int &chapter, int64_t &cur_pts) override;
  enum SeekResult SeekTime(int64_t time, int64_t *cur_pts, bool backwards = false) override;
  inline bool CanSeek() override { return true; };
  static int dvd_read(void *h, uint8_t* buf, int size);
  static int64_t dvd_seek(void *h, int64_t pos, int whence);

protected:
  OMXDvdPlayer *m_DvdPlayer = NULL;
  bool SeekBytes(int64_t seek_bytes, bool backwords);
  void AddMissingSubtitleStream(int id);
  void GetStreams() override;

  AVIOContext *m_ioContext = NULL;
};

#endif
