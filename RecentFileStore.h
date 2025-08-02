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
  void forget(const std::string &key);
  void remember(const std::string &url, const int &dvd_track, const int &pos, const std::string &audio, const int &audio_track, const std::string &subtitle, const int &subtitle_track);
  void saveStore();
  bool checkIfLink(const std::string &filename);
  void readlink(std::string &filename, int &track, int &pos, std::string &audio, int &audio_track, std::string &subtitle_lang, int &subtitle_track);
  void retrieveRecentInfo(const std::string &filename, int &track, int &pos, std::string &audio, int &audio_track, std::string &subtitle_lang, int &sub_track);

private:
  struct fileInfo {
    std::string url;
    int time = -1;
    int dvd_track = -1;
    std::string audio_lang;
    int audio_track = -1;
    std::string subtitle_lang;
    int subtitle_track = -1;
  };

  void readlink(fileInfo *f);
  void getRecentFileList(std::vector<std::string> &recents);
  void clearRecents();
  void setDataFromStruct(const fileInfo *store_item, int &dvd_track, int &pos, std::string &audio, int &audio_track, std::string &subtitle, int &subtitle_track);

  std::vector<fileInfo> store;
  std::string recent_dir;
  bool m_init = false;
};
