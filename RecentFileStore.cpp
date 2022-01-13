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
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

#include "utils/RegExp.h"
#include "RecentFileStore.h"

using namespace std;

RecentFileStore::RecentFileStore()
{
	// recent dir
	recent_dir = getenv("HOME");
	recent_dir += "/OMXPlayerRecent/"; // note the trailing slash
}

bool RecentFileStore::readStore()
{
	// create recent dir if necessary
    struct stat fileStat;
    if(stat(recent_dir.c_str(), &fileStat) == 0) {
        if(!S_ISDIR(fileStat.st_mode)) {
            // file exists but is not a directory
            puts("File blocking creation of recents directory: disabling playlist");
            return false;
        }
    } else if(mkdir(recent_dir.c_str(), 0777) != 0) {
        // file exists but is not a directory
        puts("Failed to create recents directory: disabling playlist");
        return false;
    }

	vector<string> recents = getRecentFileList();
	sort(recents.begin(), recents.end());
	uint size = recents.size();

	store.resize(size);

	for(uint i = 0; i < size; i++) {
		store[i].url = recents[i];
		readlink(&store[i]);
	}

	m_init = true;
	return true;
}

bool RecentFileStore::checkIfLink(string &filename)
{
    int start_point = filename.length() - 4;
    if(start_point > 0 && filename.substr(start_point) == ".url") {
        return true;
    }

    if(filename.length() > recent_dir.length() &&
            filename.substr(0, recent_dir.length()) == recent_dir) {
        return true;
    }

    return false;
}

bool split(string &line, string &key, string &val)
{
    string::size_type n = line.find("=");
    if(n == string::npos)
        return false;

    key = line.substr(0, n);
    val = line.substr(n + 1);
    return true;
}

void RecentFileStore::retrieveRecentInfo(string &filename, int &track, int &pos, char *audio, char *subtitle_lang, int &sub_track)
{
	for(unsigned i = 0; i < store.size(); i++) {
		if(store[i].url == filename) {
			setDataFromStruct(&store[i], track, pos, audio, subtitle_lang, sub_track);
			return;
		}
	}
}

void RecentFileStore::setDataFromStruct(fileInfo *store_item, int &dvd_track, int &pos, char *audio, char *subtitle, int &sub_track)
{
	if(dvd_track == -1)
		dvd_track = store_item->dvd_track;
	if(pos == -1)
		pos = store_item->time;
	if(audio[0] == '\0')
		strncpy(audio, store_item->audio_lang, 3);
	if(subtitle[0] == '\0')
		strncpy(subtitle, store_item->subtitle_lang, 3);
	if(store_item->subtitle_extern && sub_track == -1 && subtitle[0] == '\0')
		sub_track = 0;
}

bool is_valid_link_url(std::string url)
{
	if(url[0] == '/')
		return true;

	if(url.substr(0, 5) == "file:")
		return false;

	unsigned int i = 0;
	while(url[i] >= 'a' && url[i] <= 'z')
		i++;

	return i > 3 && url.size() > i + 3 && url.substr(i, 3) == "://";
}


void RecentFileStore::readlink(fileInfo *f)
{
	string line;
	ifstream s(f->url);

	if(getline(s, line) && is_valid_link_url(line)) {
		f->url = line;
	} else {
		f->url = "";
		return;
	}

    string key;
    string val;
	while(getline(s, line) && split(line, key, val)) {
		if(key == "pos") {
			f->time = atoi(val.c_str());
		} else if(key == "dvd_track") {
			f->dvd_track = atoi(val.c_str());
		} else if(key == "audio_lang") {
			strncpy(f->audio_lang, val.c_str(), 3);
		} else if(key == "subtitle_lang") {
			strncpy(f->subtitle_lang, val.c_str(), 3);
		}  else if(key == "subtitle_extern" && val == "1") {
			f->subtitle_extern = true;
		}
	}

	// backward compatibility
    if(line[0] >= '0' && line[0] <= '9') {
        if(f->time == -1) {
            f->time = atoi(line.c_str());
        }

        if(getline(s, line) && line[0] >= '0' && line[0] <= '9') {
            if(f->dvd_track == -1) {
                f->dvd_track = atoi(line.c_str());
            }
        }
	}

	s.close();
}

void RecentFileStore::readlink(string &filename, int &track, int &pos, char *audio, char *subtitle_lang, int &sub_track)
{
	fileInfo f;
	f.url = filename;
	readlink(&f);

	filename = f.url;
	
	setDataFromStruct(&f, track, pos, audio, subtitle_lang, sub_track);
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
	for(auto i = store.begin(); i != store.end(); i++) {
		if(i->url == key) {
			store.erase(i);
			break;
		}
	}
}

void RecentFileStore::remember(string &url, int &dvd_track, int &pos, char *audio, char *subtitle, bool &subtitle_extern)
{
	fileInfo newFile;

	newFile.url = url;

	if(pos > 0)
		newFile.time = pos;
	if(dvd_track > 0)
		newFile.dvd_track = dvd_track;
	if(audio[0] != '\0')
		strncpy(newFile.audio_lang, audio, 3);

	if(subtitle[0] != '\0')
		strncpy(newFile.subtitle_lang, subtitle, 3);
	else if(subtitle_extern)
	    newFile.subtitle_extern = true;

	store.insert(store.begin(), newFile);
}

void RecentFileStore::clearRecents()
{
	vector<string> old_recents = getRecentFileList();

	for(uint i=0; i < old_recents.size(); i++)
		std::remove(old_recents[i].c_str());
}

void RecentFileStore::saveStore()
{
	if(!m_init) return;

	// delete all existing link files
	clearRecents();

	// set up some regexes
	CRegExp link_file = CRegExp(true);
	link_file.RegComp("/([^/]+?)(\\.[^\\.]{1,4}|)$");

	CRegExp link_stream = CRegExp(true);
	link_stream.RegComp("://([^/]+)/");

	int size = store.size();
	if(size > 20) size = 20; // to to twenty files
	for(int i = 0; i < size; i++) {
		// make link name
		string link;

		if(i < 9) link += '0';
		link += to_string(i+1) + " - ";

		if(link_file.RegFind(store[i].url, 0) > -1) {
			link += link_file.GetMatch(1) + ".url";
		} else if(link_stream.RegFind(store[i].url, 0) > -1) {
			link += link_stream.GetMatch(1) + ".url";
		} else {
			link += "stream.url";
		}

		// write link file
		ofstream s(recent_dir + link);
		s << store[i].url << '\n';

		if(store[i].time > 0)
			s << "pos=" << store[i].time << "\n";
		if(store[i].dvd_track > 0)
			s << "dvd_track=" << store[i].dvd_track << "\n";
		if(store[i].audio_lang[0] != '\0')
			s << "audio_lang=" << store[i].audio_lang << "\n";

		if(store[i].subtitle_lang[0] != '\0')
			s << "subtitle_lang=" << store[i].subtitle_lang << "\n";
		else if(store[i].subtitle_extern)
			s << "subtitle_extern=1\n";

		s.close();
	}
}
