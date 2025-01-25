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
#include "OMXReaderDvd.h"
#include "OMXPacket.h"
#include "OMXDvdPlayer.h"
#include "utils/defs.h"
#include "utils/log.h"

#include <string>
#include <limits.h>
#include <stdexcept>
#include <memory>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/intreadwrite.h>
#include <libavformat/avio.h>
}

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>

#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg
#define PCI_SIZE  980

OMXReaderDvd::OMXReaderDvd(dvd_file_t *dt, OMXDvdPlayer::track_info &ct, int tn)
    :
  m_dvd_track(dt),
  m_current_track(ct),
  m_track_num(tn)
{
  CLogLog(LOGDEBUG, "COMXPlayer::OpenFile - open dvd");

  unsigned char *buffer = (unsigned char*)av_malloc(FFMPEG_FILE_BUFFER_SIZE);
  if(!buffer)
    throw "av_malloc failed";

  m_ioContext = avio_alloc_context(buffer, FFMPEG_FILE_BUFFER_SIZE, 0, this, dvd_read, NULL, dvd_seek);
  if(!m_ioContext)
    throw "avio_alloc_context failed";

  AVCONST AVInputFormat *iformat = NULL;
  av_probe_input_buffer(m_ioContext, &iformat, NULL, NULL, 0, 0);
  if(!iformat)
    throw "av_probe_input_buffer failed";

  m_pFormatContext->pb = m_ioContext;
  int result = avformat_open_input(&m_pFormatContext, NULL, iformat, &s_avdict);
  av_dict_free(&s_avdict);
  if(result < 0)
    throw "avformat_open_input failed";

  if(avformat_find_stream_info(m_pFormatContext, NULL) < 0)
    throw "avformat_find_stream_info failed";

  m_pFormatContext->duration = (int64_t)m_current_track.length * 1000;

  if(m_pFormatContext->start_time != AV_NOPTS_VALUE)
    m_prev_pack_end = m_pFormatContext->start_time * 9 / 100;

  GetStreams();
}

OMXReaderDvd::~OMXReaderDvd()
{
  if(m_pFormatContext->pb && m_pFormatContext->pb != m_ioContext)
  {
    CLogLog(LOGWARNING, "CDVDDemuxFFmpeg::Dispose - demuxer changed our byte context behind our back, possible memleak");
    m_ioContext = m_pFormatContext->pb;
  }

  av_free(m_ioContext->buffer);
  avio_context_free(&m_ioContext);
}

OMXPacket *OMXReaderDvd::Read()
{
again:
  OMXPacket *pkt = OMXReader::Read();

  if(pkt == NULL)
    return NULL;

  // check for dvd_nav_packets
  if(pkt->hints.codec == AV_CODEC_ID_DVD_NAV)
  {
    pci_t pci_pack;

    navRead_PCI(&pci_pack, &pkt->avpkt->data[1]);

    if(m_prev_pack_end != pci_pack.pci_gi.vobu_s_ptm)
    {
        int64_t diff = (int64_t)m_prev_pack_end - (int64_t)pci_pack.pci_gi.vobu_s_ptm;
        m_offset += diff * 100 / 9;
        printf("Offset: %lld\n", m_offset);
    }

    m_prev_pack_end = pci_pack.pci_gi.vobu_e_ptm;

    delete pkt;
    goto again;
  }

  if(pkt->avpkt->pts != AV_NOPTS_VALUE)
    pkt->avpkt->pts += m_offset;

  if(pkt->avpkt->dts != AV_NOPTS_VALUE)
    pkt->avpkt->dts += m_offset;

  return pkt;
}

int OMXReaderDvd::AddStream(int id, const char *lang)
{
  const AVStream *pStream = m_pFormatContext->streams[id];

  // check to see if we are waiting for this stream
  int idx;
  try {
    idx = m_pending_streams.at(pStream->id);
  }
  catch(std::out_of_range const&)
  {
    return OMXReader::AddStream(id, lang);
  }

  // this is a preallocated subtitle stream
  OMXStream *this_stream   = &m_streams[OMXSTREAM_SUBTITLE][idx];
  m_steam_map[id]          = idx;

  PopulateStream(id, lang, this_stream);

  // return the stream type index
  return m_steam_map[id];
}

enum SeekResult OMXReaderDvd::SeekTimeDelta(int64_t delta, int64_t &cur_pts)
{
  int64_t seek_pts = cur_pts + delta;
  if(seek_pts < 0) return SEEK_OUT_OF_BOUNDS;

  enum SeekResult r = SeekTime(seek_pts, delta < 0);

  if(r == SEEK_SUCCESS)
    cur_pts = seek_pts;

  return r;
}

bool OMXReaderDvd::SeekByte(int seek_byte, bool backwords, const int64_t &new_pts)
{
  m_ioContext->buf_ptr = m_ioContext->buf_end;

  int flags = (backwords ? AVSEEK_FLAG_BACKWARD : 0) | AVSEEK_FLAG_BYTE;
  reset_timeout(1);

  if(av_seek_frame(m_pFormatContext, -1, (int64_t)seek_byte * 2048, flags) < 0)
  {
    m_eof = true;
    return false;
  }

  m_offset = 0;
  int64_t time = new_pts;
  if(m_pFormatContext->start_time != AV_NOPTS_VALUE)
    time += m_pFormatContext->start_time;

  m_prev_pack_end = time * 9 / 100;
  m_eof = false;
  return true;
}


void OMXReaderDvd::GetStreams()
{
  std::unordered_map<int, int> stream_lookup;
  for(unsigned int i = 0; i < m_pFormatContext->nb_streams; i++)
    stream_lookup[m_pFormatContext->streams[i]->id] = i;

  auto AddStreamByHexId = [&](int id, const char *lang = NULL)
  {
    try {
      OMXReader::AddStream(stream_lookup.at(id), lang);
      return true;
    }
    catch(std::out_of_range const&) {
      return false;
    }
  };

  // assume video is always 0x1e0
  AddStreamByHexId(0x1e0);

  // audio streams
  for(auto audio_stream : m_current_track.title->audio_streams)
    AddStreamByHexId(audio_stream.id, OMXDvdPlayer::convertLangCode(audio_stream.lang));

  // subtitle streams
  for(auto subtitle_stream : m_current_track.title->subtitle_streams)
  {
    int hex_id = subtitle_stream.id;
    const char *lang = OMXDvdPlayer::convertLangCode(subtitle_stream.lang);

    if(!AddStreamByHexId(hex_id, lang))
      AddMissingSubtitleStream(hex_id, lang);
  }
}


SeekResult OMXReaderDvd::SeekChapter(int delta, int &result_ch, int64_t &cur_pts)
{
  if(cur_pts == AV_NOPTS_VALUE) return SEEK_FAIL;

  result_ch = GetCell(cur_pts / 1000);
  if(result_ch == -1)
    return SEEK_OUT_OF_BOUNDS;

  // find the next/prev cell that's a cell
  do {
    result_ch += delta;

    // check if within bounds
    if(result_ch < 0 || result_ch >= (int)m_current_track.cells.size())
      return SEEK_OUT_OF_BOUNDS;

  } while(!m_current_track.cells[result_ch].is_chapter);

  cur_pts  = (int64_t)m_current_track.cells[result_ch].time * 1000;

  // Do the seek
  if(!SeekByte(m_current_track.cells[result_ch].cell, delta < 0, cur_pts))
    return SEEK_FAIL;

  return SEEK_SUCCESS;
}


void OMXReaderDvd::AddMissingSubtitleStream(int hex_id, const char *lang)
{
  m_pending_streams[hex_id]       = m_streams[OMXSTREAM_SUBTITLE].size();
  OMXStream &this_stream          = m_streams[OMXSTREAM_SUBTITLE].emplace_back();
  this_stream.type                = OMXSTREAM_SUBTITLE;
  this_stream.codec_name          = "dvdsub";
  this_stream.hints.codec         = AV_CODEC_ID_DVD_SUBTITLE;
  this_stream.language            = lang;
}


int OMXReaderDvd::dvd_read(void *h, uint8_t* buf, int size)
{
  OMXReaderDvd *reader = static_cast<OMXReaderDvd*>(h);
  return reader->DvdRead(buf, size / DVD_VIDEO_LB_LEN);
}

int64_t OMXReaderDvd::dvd_seek(void *h, int64_t new_pos, int whence)
{
  OMXReaderDvd *reader = static_cast<OMXReaderDvd*>(h);
  return reader->DvdSeek(new_pos / DVD_VIDEO_LB_LEN, whence);
}

int OMXReaderDvd::DvdRead(uint8_t *lpBuf, int blocks_to_read)
{
  reset_timeout(1);
  if(interrupt_cb())
    return -1;

  if(m_pos + blocks_to_read > m_current_track.parts[m_current_part].blocks) {
    blocks_to_read = m_current_track.parts[m_current_part].blocks - m_pos;

    if(blocks_to_read < 1)
      return AVERROR_EOF;
  }

  int read_start = m_current_track.parts[m_current_part].first_sector + m_pos;
  int read_blocks = DVDReadBlocks(m_dvd_track, read_start, blocks_to_read, lpBuf);

  // return any error
  // NB treat a zero as an error since we've just checked that there are cells to read
  if(read_blocks < 0)
  {
    printf("OMXReaderDvd::DvdRead failed: %d - %d - %d - %d\n", m_track_num, m_current_part, read_start, blocks_to_read);
    return -1;
  }

  // move internal pointers forward
  m_pos += read_blocks;
  if(m_pos == m_current_track.parts[m_current_part].blocks
      && m_current_part < (int)m_current_track.parts.size() - 1)
  {
    m_current_part++;
    m_pos = 0;
  }

  return read_blocks * DVD_VIDEO_LB_LEN;
}

uint32_t *OMXReaderDvd::getPalette(OMXStream *st, uint32_t *palette)
{
  return m_current_track.title->palette;
}

int64_t OMXReaderDvd::DvdSeek(int new_pos, int whence)
{
  reset_timeout(1);
  if(interrupt_cb())
    return -1;

  switch(whence)
  {
  case SEEK_SET:
    m_pos = new_pos;
    m_current_part = 0;

    // find the part of the current track we are seeking to and adjust the
    // m_pos to be within that part
    while(m_current_part + 1 < (int)m_current_track.parts.size()
        && m_pos >= m_current_track.parts[m_current_part].blocks)
    {
      m_pos -= m_current_track.parts[m_current_part].blocks;
      m_current_part++;
    }

    return new_pos < 0 ? new_pos : (int64_t)new_pos * DVD_VIDEO_LB_LEN;

  case AVSEEK_SIZE:
    {
      int total_blocks = 0;
      for(auto part : m_current_track.parts)
        total_blocks += part.blocks;

      return (int64_t)total_blocks * DVD_VIDEO_LB_LEN;
    }
  default:
    return -1;
  }
}

enum SeekResult OMXReaderDvd::SeekTime(int64_t &seek_micro, bool backwards)
{
  // save these in case they have to be restored later
  int old_pos = m_pos;
  int old_current_part = m_current_part;

  int seek_ms = seek_micro / 1000;
  dsi_t dsi_pack;
  unsigned char data[DVD_VIDEO_LB_LEN];
  const int list[] = {0, 500, 8000, 10000, 30000, 60000, 120000};

  int ch = GetCell(seek_ms);
  if(ch == -1)
  {
    m_eof = true;
    return SEEK_OUT_OF_BOUNDS;
  }

  int cur_time_seeking_pos;
  int cur_cell_seeking_pos = m_current_track.cells[ch].cell;
  int seek_within_cell = seek_ms - m_current_track.cells[ch].time;

  // limit to 12 jumps to avoid indef loops
  for(int i = 0; i < 12; i++)
  {
    DvdSeek(cur_cell_seeking_pos);
    if(DvdRead(data, 1) != DVD_VIDEO_LB_LEN)
    {
      printf("read error: %d\n", cur_cell_seeking_pos);
      m_pos = old_pos;
      m_current_part = old_current_part;
      goto finish;
    }

    navRead_DSI(&dsi_pack, &data[DSI_START_BYTE]);
    cur_time_seeking_pos = OMXDvdPlayer::dvdtime2msec(&dsi_pack.dsi_gi.c_eltm);

    int diff = seek_within_cell - cur_time_seeking_pos;

    int lower = 0;
    int higher = 7;

    while(higher - lower > 1)
    {
      int m = (lower + higher) / 2;

      if(diff >= list[m])
        lower = m;
      else
        higher = m;
    }

    int jump;
    switch(lower)
    {
    case 0:
      goto finish;

    case 1:
      jump = 19 - (diff / 500);
      break;

    default:
      jump = 6 - lower;
      break;
    }

    // the two most significants bits indicate meta data
    // the rest constitute the offset to another vobu
    // 0x3fffffff means there's not vobu to jump to
    int next = dsi_pack.vobu_sri.fwda[jump] & 0x3fffffff;
    if(next == 0x3fffffff || next <= 0)
      goto finish;

    cur_cell_seeking_pos += next;
  }

finish:
  // save seek time
  int seeked_ms = m_current_track.cells[ch].time + cur_time_seeking_pos;
  seek_micro = (int64_t)seeked_ms * (int64_t)1000;

  // now do the "actual" seek using ffmpeg
  bool success = SeekByte(cur_cell_seeking_pos, backwards, seek_micro);

  // restore prev position on failure
  if(!success)
  {
    m_pos = old_pos;
    m_current_part = old_current_part;
  }

  // demuxer will return failure, if you seek to eof
  m_eof = !success;

  return success ? SEEK_SUCCESS : SEEK_FAIL;
}

// search for the cell the timestamp is in
int OMXReaderDvd::GetCell(int needle)
{
  int l = 0;
  int r = (int)m_current_track.cells.size();

  if(needle < 0 || needle >= m_current_track.length)
    return -1;

  while(r - l > 1)
  {
    int m = (l + r) / 2;

    if(needle >= m_current_track.cells[m].time)
      l = m;
    else
      r = m;
  }

  return l;
}
