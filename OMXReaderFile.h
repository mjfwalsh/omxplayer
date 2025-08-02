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

#include "OMXReader.h"
#include "OMXReaderFile.h"

#include <string>
#include <stdint.h>

#define MAX_OMX_CHAPTERS 64

class OMXReaderFile : public OMXReader
{
public:
  OMXReaderFile(std::string &filename, bool live, bool has_extern);

  bool CanSeek() override;
  SeekResult SeekChapter(int delta, int &result_chapter, int64_t &cur_pts) override;
  enum SeekResult SeekTime(int64_t &time, bool backwards) override;
  enum SeekResult SeekTimeDelta(int64_t delta, int64_t &cur_pts) override;

protected:
  void GetStreams();
  void GetChapters();
  uint32_t *getPalette(OMXStream *st, uint32_t *palette) override;
  void AddExternalSubs();

  int64_t m_chapters[MAX_OMX_CHAPTERS];
  int m_chapter_count = 0;
};
