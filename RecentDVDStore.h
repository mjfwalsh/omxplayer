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

class RecentDVDStore
{
public:
	void readStore();
	void setCurrentDVD(const std::string &key, int &track, int &time, char *audio, char *subtitle);
	void remember(int &track, int &time, char *audio, char *subtitle);
	void saveStore();

private:
	struct DVDInfo {
		std::string key;
		int time = -1;
		int track = -1;
		std::string audio;
		std::string subtitle;
	};

	std::vector<DVDInfo> store;
	std::string recent_dvd_file;
	std::string current_dvd;
	bool m_init = false;
};
