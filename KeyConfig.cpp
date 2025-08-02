/*
 * Copyright (C) 2022 by Michael J. Walsh
 * Copyright (C) 2013 by Mitch Draves
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

#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string.h>

#include "KeyConfig.h"

using namespace std;

namespace KeyConfig {

struct action_lookup
{
  const char* k;
  enum Action v;
};

const struct action_lookup table[] = {
// NB this list is CASE SENSITIVELY sorted
  {"DECREASE_SPEED", ACTION_DECREASE_SPEED},
  {"DECREASE_SUBTITLE_DELAY", ACTION_DECREASE_SUBTITLE_DELAY},
  {"DECREASE_VOLUME", ACTION_DECREASE_VOLUME},
  {"EXIT", ACTION_EXIT},
  {"HIDE_SUBTITLES", ACTION_HIDE_SUBTITLES},
  {"INCREASE_SPEED", ACTION_INCREASE_SPEED},
  {"INCREASE_SUBTITLE_DELAY", ACTION_INCREASE_SUBTITLE_DELAY},
  {"INCREASE_VOLUME", ACTION_INCREASE_VOLUME},
  {"NEXT_AUDIO", ACTION_NEXT_AUDIO},
  {"NEXT_CHAPTER", ACTION_NEXT_CHAPTER},
  {"NEXT_FILE", ACTION_NEXT_FILE},
  {"NEXT_SUBTITLE", ACTION_NEXT_SUBTITLE},
  {"PAUSE", ACTION_PLAYPAUSE},
  {"PREVIOUS_AUDIO", ACTION_PREVIOUS_AUDIO},
  {"PREVIOUS_CHAPTER", ACTION_PREVIOUS_CHAPTER},
  {"PREVIOUS_FILE", ACTION_PREVIOUS_FILE},
  {"PREVIOUS_SUBTITLE", ACTION_PREVIOUS_SUBTITLE},
  {"SEEK_BACK_LARGE", ACTION_SEEK_BACK_LARGE},
  {"SEEK_BACK_SMALL", ACTION_SEEK_BACK_SMALL},
  {"SEEK_FORWARD_LARGE", ACTION_SEEK_FORWARD_LARGE},
  {"SEEK_FORWARD_SMALL", ACTION_SEEK_FORWARD_SMALL},
  {"SHOW_SUBTITLES", ACTION_SHOW_SUBTITLES},
  {"STEP", ACTION_STEP},
  {"TOGGLE_SUBTITLE", ACTION_TOGGLE_SUBTITLE},
};

const int item_size = sizeof(const action_lookup);
const int item_count = sizeof(table) / sizeof(const action_lookup);

static int compare(const void *a, const void *b)
{
  const char *ia = (const char *)a;
  const struct action_lookup *ib = (const struct action_lookup *)b;

  return strcmp(ia, ib->k);
}

/* Converts the action string from the config file into
 * the corresponding enum value
 */
static int convertStringToAction(const string &str_action)
{
  struct action_lookup *result = (struct action_lookup *)
    bsearch(str_action.c_str(), table, item_count, item_size, compare);

  return result == nullptr ? -1 : result->v;
}
/* Parses a line from the config file in the mode 'action:key'. Looks up
the action in the relevant enum array. Returns true on success. */
static bool getActionAndKeyFromString(const string &line, int &int_action, string &key)
{
  if(line[0] == '#')
    return false;

  unsigned int colonIndex = line.find(":");
  if(colonIndex == string::npos)
    return false;

  string str_action = line.substr(0, colonIndex);
  key = line.substr(colonIndex+1);

  int_action = convertStringToAction(str_action);

  if(int_action == -1 || key.size() < 1)
    return false;

  return true;
}

/* Returns a keymap consisting of the default
 *  keybinds specified with the -k option
 */
static void buildDefaultKeymap(unordered_map<int,int> &keymap)
{
  keymap['<'] = ACTION_DECREASE_SPEED;
  keymap['>'] = ACTION_INCREASE_SPEED;
  keymap[','] = ACTION_DECREASE_SPEED;
  keymap['.'] = ACTION_INCREASE_SPEED;
  keymap['j'] = ACTION_PREVIOUS_AUDIO;
  keymap['k'] = ACTION_NEXT_AUDIO;
  keymap['i'] = ACTION_PREVIOUS_CHAPTER;
  keymap['o'] = ACTION_NEXT_CHAPTER;
  keymap['9'] = ACTION_PREVIOUS_FILE;
  keymap['0'] = ACTION_NEXT_FILE;
  keymap['n'] = ACTION_PREVIOUS_SUBTITLE;
  keymap['m'] = ACTION_NEXT_SUBTITLE;
  keymap['s'] = ACTION_TOGGLE_SUBTITLE;
  keymap['d'] = ACTION_DECREASE_SUBTITLE_DELAY;
  keymap['f'] = ACTION_INCREASE_SUBTITLE_DELAY;
  keymap['q'] = ACTION_EXIT;
  keymap[KEY_ESC] = ACTION_EXIT;
  keymap['p'] = ACTION_PLAYPAUSE;
  keymap[' '] = ACTION_PLAYPAUSE;
  keymap['-'] = ACTION_DECREASE_VOLUME;
  keymap['+'] = ACTION_INCREASE_VOLUME;
  keymap['='] = ACTION_INCREASE_VOLUME;
  keymap[KEY_LEFT] = ACTION_SEEK_BACK_SMALL;
  keymap[KEY_RIGHT] = ACTION_SEEK_FORWARD_SMALL;
  keymap[KEY_DOWN] = ACTION_SEEK_BACK_LARGE;
  keymap[KEY_UP] = ACTION_SEEK_FORWARD_LARGE;
  keymap['v'] = ACTION_STEP;
  keymap['w'] = ACTION_SHOW_SUBTITLES;
  keymap['x'] = ACTION_HIDE_SUBTITLES;
}

/* Parses the supplied config file and turns it into a map object.
 */
static void parseConfigFile(const char *filepath, unordered_map<int, int> &keymap)
{
  ifstream config_file(filepath);

  if(!config_file.is_open())
  {
    cerr << "Failed to open key config file: " << filepath << endl;
    buildDefaultKeymap(keymap);
    return;
  }

  string line;
  int key_action;
  string key;

  while(getline(config_file, line))
  {
    if(getActionAndKeyFromString(line, key_action, key))
    {
      if(key.substr(0,4) == "left")
      {
        keymap[KEY_LEFT] = key_action;
      }
      else if(key.substr(0,5) == "right")
      {
        keymap[KEY_RIGHT] = key_action;
      }
      else if(key.substr(0,2) == "up")
      {
        keymap[KEY_UP] = key_action;
      }
      else if(key.substr(0,4) == "down")
      {
        keymap[KEY_DOWN] = key_action;
      }
      else if(key.substr(0,3) == "esc")
      {
        keymap[KEY_ESC] = key_action;
      }
      else if(key.substr(0,5) == "space")
      {
        keymap[' '] = key_action;
      }
      else if(key.substr(0,3) == "num" || key.substr(0,3) == "hex")
      {
        if(key.size() > 4)
        {
        int n = strtoul(key.substr(4).c_str(), nullptr, 0);
        if (n > 0)
          keymap[n] = key_action;
        }
      }
      else
      {
        // this deals with unicode chars
        int len = std::min((int)key.length(), 4);
        int k = key[0];

        for(int i = 1; i < len; i++)
        k = (k << 8) | key[i];

        keymap[k] = key_action;
      }
    }
  }
}

void buildKeymap(const char *filename, unordered_map<int, int> &map)
{
  if(filename == nullptr) {
    buildDefaultKeymap(map);
  } else {
    parseConfigFile(filename, map);
  }
}

}
