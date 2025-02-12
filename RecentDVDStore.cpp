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
#include <fstream>
#include <string.h>

#include "RecentDVDStore.h"

using namespace std;

static vector<string> split(const string &text)
{
  int start = 0;
  vector<string> tokens;

  int s = text.size();
  if(text[s-1] == '\n') s--;

  int i = 0;
  for(; i < s; i++) {
    if(text[i] == '\t') {
      tokens.push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  tokens.push_back(text.substr(start, i - start));
  return tokens;
}

void RecentDVDStore::readStore()
{
  // find recent DVD file store
  char *home = getenv("HOME");
  if(!home)
  {
    puts("Failed to identify home directory");
    return;
  }

  recent_dvd_file.assign(home);
  recent_dvd_file += "/.omxplayer_dvd_store";

  // mark as initialised even if the recent file doesn't exist
  m_init = true;

  // open file
  ifstream s(recent_dvd_file);
  if(!s) return;

  string line;
  while(getline(s, line)) {
    vector<string> tokens = split(line);

    if(tokens.size() == 5) {
      DVDInfo &d = store.emplace_back();

      d.key = tokens[0];
      d.track = atoi(tokens[1].c_str());
      d.time = atoi(tokens[2].c_str());
      d.audio = tokens[3];
      d.subtitle = tokens[4];
    }
  }
  s.close();
}


void RecentDVDStore::retrieveRecentInfo(const string &key, int &track, int &time, char *audio, char *subtitle)
{
  if(!m_init) return;

  current_dvd = key;

  if(current_dvd.empty()) return;

  for(auto i = store.begin(); i != store.end(); i++) {
    if(i->key == key) {
      if(time == -1 && track == -1) {
        track = i->track;
        time = i->time;
      } else if(track == i->track) {
        time = i->time;
      }
      if(audio[0] == '\0') {
        strncpy(audio, i->audio.c_str(), 3);
      }
      if(subtitle[0] == '\0') {
        strncpy(subtitle, i->subtitle.c_str(), 3);
      }
      store.erase(i);
      return;
    }
  }
}


void RecentDVDStore::remember(const int &track, const int &time, char *audio, char *subtitle)
{
  if(!m_init || current_dvd.empty()) return;

  DVDInfo d;
  d.key = current_dvd;
  d.time = time;
  d.track = track;
  d.audio = audio;
  d.subtitle = subtitle;

  store.insert(store.begin(), d);
}

void RecentDVDStore::saveStore()
{
  if(!m_init) return;

  int size = store.size();
  if(size > 20) size = 20; // to to twenty files

  ofstream s(recent_dvd_file);

  for(int i = 0; i < size; i++) {
    s << store[i].key << '\t';
    s << store[i].track << '\t';
    s << store[i].time << '\t';
    s << store[i].audio << '\t';
    s << store[i].subtitle << '\n';
  }

  s.close();

  store.clear();
  m_init = false;
}
