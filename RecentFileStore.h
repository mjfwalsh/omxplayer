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
	void readStore();
	void forget(string &key);
	int getTime(string &key, int &track);
	void remember(string key, int track, int time);
	void saveStore();
	bool checkIfRecentFile(string &filename);

private:
	struct fileInfo {
		int time;
		int track;
		int pos;
	};

	vector<string> getRecentFileList();
	void clearRecents();
	static bool fileinfoCmp(pair<string, fileInfo> const &a, pair<string, fileInfo> const &b);

	map<string, fileInfo> store;
	string recent_dir;
	bool m_init = false;
};
