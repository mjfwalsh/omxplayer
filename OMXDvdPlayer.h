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
#include <string>

class DVD;

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

	int Read(unsigned char *lpBuf, int64_t uiBufSize);
	int64_t Seek(int64_t iFilePosition, int iWhence);
	int getChapter(int64_t timestamp);
	int GetChapterInfo(int64_t &seek_ts, int64_t &byte_pos);
	int64_t GetChapterBytePos(int seek_ch);
	bool IsEOF();
	int64_t GetSizeInBytes();
	int getCurrentTrackLength();
	int TotalChapters();
	int GetAudioStreamCount();
	int GetSubtitleStreamCount();
	int GetAudioStreamId(int i);
	int GetSubtitleStreamId(int i);
	const char *GetAudioStreamLanguage(int i);
	const char *GetSubtitleStreamLanguage(int i);
	std::string GetID() const { return disc_checksum; }
	std::string GetTitle() const { return disc_title; }
	void enableHeuristicTrackSelection();
	uint32_t *getPalette();

  private:
	int dvdtime2msec(dvd_time_t *dt);
	const char* convertLangCode(uint16_t lang);
	void read_title_name();
	void read_disc_checksum();
	void read_disc_serial_number();

	bool m_open = false;
	int pos = 0;
	int pos_byte_offset = 0;
	DVD *dvdread;

	dvd_reader_t *dvd_device;
	dvd_file_t *dvd_track;

	int current_track = -1;
	int total_blocks = 0;

	std::string device_path;
	std::string disc_title;
	std::string disc_checksum;

	typedef struct title_info {
		int title_num;
		int audiostream_count;
		struct stream_info {
			int id; // as in the hex number in "Stream #0:2[0x85]:"
			uint16_t lang; // lang code
		} *audio_streams;
		int subtitle_count;
		struct stream_info *subtitle_streams;
		uint32_t palette[16];
		int refcount;
	} title_info;

	int track_count = 0;
	struct track_info {
		bool enabled;
		title_info *title;
		int length;
		int chapter_count;
		struct chapter_info {
			int cell;
			int time;
		} *chapters;
		int first_sector;
		int last_sector;
	} *tracks;

	float frames_per_s[4] = {-1.0, 25.00, -1.0, 29.97};

	int yvu2rgb(int c);
};
