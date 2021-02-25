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

#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <dirent.h>
#include <sys/stat.h>

#include "utils/RegExp.h"
#include "RecentFileStore.h"

using namespace std;

RecentFileStore::RecentFileStore()
{
	// recent dir
	recent_dir = getenv("HOME");
	recent_dir += "/OMXPlayerRecent/"; // note the trailing slash

	// create recent dir if necessary
    struct stat fileStat;
    if ( stat(recent_dir.c_str(), &fileStat) || !S_ISDIR(fileStat.st_mode) ) {
        mkdir(recent_dir.c_str(), 0777);
    }
}

void RecentFileStore::readStore()
{
	string uri;
	int time;
	int track;

	vector<string> recents = getRecentFileList();
	sort(recents.begin(), recents.end());

	int count = 0;
	for(uint i=0; i < recents.size(); i++) {
		ifstream s(recents[i]);

		if(getline(s, uri) && s >> time) {
			if(s >> track) {
				store[uri] = {time, track, ++count};
			} else {
				store[uri] = {time, -1, ++count};
			}
		}
		s.close();
	}

	m_init = true;
}

bool RecentFileStore::checkIfRecentFile(string &filename)
{
	if(filename.length() > recent_dir.length()
			&& filename.substr(0, recent_dir.length()) == recent_dir) {
		ifstream s(filename);

		string uri;
		bool r = (bool)getline(s, uri);
		s.close();
		
		if(r) {
			filename = uri;
			return true;
		} else {
			filename = ""; // error
			return false;
		}
	} else {
		return false;
	}
}

vector<string> RecentFileStore::getRecentFileList()
{
	vector<string> recents;

	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir(recent_dir.c_str())) == NULL)
		return recents;

	// re for filename match
	CRegExp link_file = CRegExp(true);
	link_file.RegComp("^[0-9]{2} - ");

	while ((ent = readdir (dir)) != NULL) {
		if(link_file.RegFind(ent->d_name, 0) > -1) {
			recents.push_back(recent_dir + ent->d_name);
		}
	}

	return recents;
}

void RecentFileStore::forget(string &key)
{
	store.erase(key);
}

int RecentFileStore::getTime(string &key, int &track)
{
	if(store.find(key) == store.end()) {
		return 0;
	}

	if(track == -1) {
		track = store[key].track;
		return store[key].time;
	}

	if(track == store[key].track) {
		return store[key].time;
	}

	return 0;
}

void RecentFileStore::remember(string key, int track, int time)
{
	store[key] = {time, track, -1};
}

void RecentFileStore::clearRecents()
{
	vector<string> old_recents = getRecentFileList();

	for(uint i=0; i < old_recents.size(); i++)
		std::remove(old_recents[i].c_str());
}

bool RecentFileStore::fileinfoCmp(pair<string, fileInfo> const &a, pair<string, fileInfo> const &b)
{
	return a.second.pos < b.second.pos;
}

void RecentFileStore::saveStore()
{
	if(!m_init) return;

	// delete all existing link files
	clearRecents();

	// set up some regexes
	CRegExp link_file = CRegExp(true);
	link_file.RegComp("/([^/]+)$");

	CRegExp link_stream = CRegExp(true);
	link_stream.RegComp("://([^/]+)/");

	// create a sorted vector
	vector<pair<string, fileInfo> > vector_store;
	vector_store.assign(store.begin(), store.end());
	sort(vector_store.begin(), vector_store.end(), fileinfoCmp);

	int size = vector_store.size();
	if(size > 20) size = 20; // to to twenty files
	for(int i = 0; i < size; i++) {
		// make link name
		string link;

		if(i < 9) link += '0';
		link += to_string(i+1) + " - ";

		if(link_file.RegFind(vector_store[i].first, 0) > -1) {
			link += link_file.GetMatch(1);
		} else if(link_stream.RegFind(vector_store[i].first, 0) > -1) {
			link += link_stream.GetMatch(1) + ".ts";
		} else {
			link += "stream.ts";
		}

		// write link file
		ofstream s(recent_dir + link);
		s << vector_store[i].first << '\n' << vector_store[i].second.time << "\n";
		if(vector_store[i].second.track > 0) s << vector_store[i].second.track << "\n";
		s.close();
	}
}
