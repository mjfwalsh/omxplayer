#pragma once
/*
 * Copyright (C) 2022 by Michael J. Walsh
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdnav; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <vector>
#include <string>
#include <memory>

class OMXDvdPlayer
{
  public:
	explicit OMXDvdPlayer(const std::string &filename);
	~OMXDvdPlayer();

	OMXDvdPlayer(const OMXDvdPlayer&) = delete;
	OMXDvdPlayer& operator=(const OMXDvdPlayer&) = delete;

	void CloseTrack();
	bool ChangeTrack(int delta, int &t);
	bool OpenTrack(int ct);

	int Read(unsigned char *lpBuf, int no_blocks);
	int Seek(int blocks, int whence = SEEK_SET);
	int getVobu(int64_t &seek_micro);
	bool PrepChapterSeek(int delta, int &seek_ch, int64_t &cur_pts, int &cell_pos);
	bool IsEOF();
	int getCurrentTrackLength();
	int GetAudioStreamCount();
	int GetSubtitleStreamCount();
	int GetAudioStreamId(int i);
	int GetSubtitleStreamId(int i);
	const char *GetAudioStreamLanguage(int i);
	const char *GetSubtitleStreamLanguage(int i);
	std::string GetID() const { return disc_checksum; }
	std::string GetTitle() const { return disc_title; }
	void removeCompositeTracks();
	uint32_t *getPalette();
	void info_dump();

  private:
	int GetCell(int ms);
	int dvdtime2msec(dvd_time_t *dt);
	const char* convertLangCode(uint16_t lang);
	void read_title_name();
	void read_disc_checksum();
	void read_disc_serial_number();

	bool m_open = false;
	int pos = 0;
	int pos_byte_offset = 0;

	dvd_reader_t *dvd_device;
	dvd_file_t *dvd_track = NULL;

	int current_track = -1;
	int current_part = -1;

	std::string device_path;
	std::string disc_title;
	std::string disc_checksum;

	typedef struct {
		unsigned int first_sector;
		int blocks;
	} part_info;

	typedef struct {
		int id; // as in the hex number in "Stream #0:2[0x85]:"
		uint16_t lang; // lang code
	} stream_info;

	typedef struct {
		int title_num;
		std::vector<stream_info> audio_streams;
		std::vector<stream_info> subtitle_streams;
		uint32_t palette[16];
	} title_info;

	typedef struct {
		int cell;
		int time;
		bool is_chapter;
	} cell_info;

	typedef struct {
		std::shared_ptr<title_info> title;
		int length;
		std::vector<cell_info> cells;
		std::vector<part_info> parts;
	} track_info;

	std::vector<track_info> tracks;

	int yvu2rgb(int c);
};
