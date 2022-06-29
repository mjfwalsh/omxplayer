#pragma once
/*
 *
 *      Copyright (C) 2020 Michael J. Walsh
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

#include <string>
#include <vector>

class RecentFileStore
{
public:
	RecentFileStore();
	bool readStore();
	void forget(std::string &key);
	void remember(std::string &url, int &dvd_track, int &pos, char *audio, int &audio_track, char *subtitle, int &subtitle_track);
	void saveStore();
	bool checkIfLink(std::string &filename);
	void readlink(std::string &filename, int &track, int &pos, char *audio, int &audio_track, char *subtitle_lang, int &subtitle_track);
	void retrieveRecentInfo(std::string &filename, int &track, int &pos, char *audio, int &audio_track, char *subtitle_lang, int &sub_track);

private:
	struct fileInfo {
		std::string url;
		int time = -1;
		int dvd_track = -1;
		char audio_lang[4] = "";
		int audio_track = -1;
		char subtitle_lang[4] = "";
		int subtitle_track = -1;
	};

	void readlink(fileInfo *f);
	std::vector<std::string> getRecentFileList();
	void clearRecents();
	void setDataFromStruct(fileInfo *store_item, int &dvd_track, int &pos, char *audio, int &audio_track, char *subtitle, int &subtitle_track);

	std::vector<fileInfo> store;
	std::string recent_dir;
	bool m_init = false;
};
