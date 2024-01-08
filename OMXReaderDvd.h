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

#include <stdint.h>
#include <unordered_map>

#include "OMXReader.h"
#include "OMXDvdPlayer.h"

class OMXReaderDvd : public OMXReader
{
public:
  OMXReaderDvd(dvd_file_t *dt, OMXDvdPlayer::track_info &ct, int tn);
  ~OMXReaderDvd();

  OMXPacket *Read() override;
  SeekResult SeekChapter(int delta, int &result_chapter, int64_t &cur_pts) override;
  enum SeekResult SeekTime(int64_t &time, bool backwards) override;
  enum SeekResult SeekTimeDelta(int64_t delta, int64_t &cur_pts) override;
  inline bool CanSeek() override { return true; };

protected:
  static int dvd_read(void *h, uint8_t* buf, int size);
  static int64_t dvd_seek(void *h, int64_t new_pos, int whence);
  bool SeekByte(int seek_byte, bool backwords, const int64_t &new_pts);
  void AddMissingSubtitleStream(int id, const char *lang);
  void GetStreams() override;
  int AddStream(int id, const char* lang = NULL) override;
  int DvdRead(unsigned char *lpBuf, int no_blocks);
  int64_t DvdSeek(int blocks, int whence = SEEK_SET);
  bool IsEOF();
  int GetCell(int ms);
  uint32_t *getPalette(OMXStream *st, uint32_t *palette) override;

  AVIOContext *m_ioContext = NULL;
  int m_pos = 0;
  int pos_byte_offset = 0;
  dvd_file_t *m_dvd_track = NULL;
  OMXDvdPlayer::track_info &m_current_track;
  int m_track_num;
  int m_current_part = 0;
  uint32_t m_prev_pack_end = 0;
  int64_t m_offset = 0;
  std::unordered_map<int, int> m_pending_streams;
};

#endif
