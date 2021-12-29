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

using namespace std;

class RecentFileStore
{
public:
	RecentFileStore();
	bool readStore();
	void forget(string &key);
	void remember(string &url, int &dvd_track, int &pos, char *audio, char *subtitle, bool &subtitle_extern);
	void saveStore();
	bool checkIfRecentFile(string &filename);
	bool readlink(string &filename, int &track, int &pos, char *audio, char *subtitle, bool &subtitle_extern);

private:
	struct fileInfo {
		string url;
		int time = -1;
		int dvd_track = -1;
		char audio_lang[4] = "";
		char subtitle_lang[4] = "";
		bool subtitle_extern = false;
	};

	bool readlink(fileInfo *f);
	vector<string> getRecentFileList();
	void clearRecents();

	vector<fileInfo> store;
	string recent_dir;
	bool m_init = false;
};
