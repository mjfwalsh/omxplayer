#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <stdlib.h>

#include "KeyConfig.h"

using namespace std;

/* Converts the action string from the config file into 
 * the corresponding enum value
 */
int convertStringToAction(const string &str_action)
{
    if(str_action == "DECREASE_SPEED")
        return ACTION_DECREASE_SPEED;
    if(str_action == "INCREASE_SPEED")
        return ACTION_INCREASE_SPEED;
    if(str_action == "PREVIOUS_AUDIO")
        return ACTION_PREVIOUS_AUDIO;
    if(str_action == "NEXT_AUDIO")
        return ACTION_NEXT_AUDIO;
    if(str_action == "PREVIOUS_CHAPTER")
        return ACTION_PREVIOUS_CHAPTER;
    if(str_action == "NEXT_CHAPTER")
        return ACTION_NEXT_CHAPTER;
    if(str_action == "PREVIOUS_FILE")
        return ACTION_PREVIOUS_FILE;
    if(str_action == "NEXT_FILE")
        return ACTION_NEXT_FILE;
    if(str_action == "PREVIOUS_SUBTITLE")
        return ACTION_PREVIOUS_SUBTITLE;
    if(str_action == "NEXT_SUBTITLE")
        return ACTION_NEXT_SUBTITLE;
    if(str_action == "TOGGLE_SUBTITLE")
        return ACTION_TOGGLE_SUBTITLE;
    if(str_action == "DECREASE_SUBTITLE_DELAY")
        return ACTION_DECREASE_SUBTITLE_DELAY;
    if(str_action == "INCREASE_SUBTITLE_DELAY")
        return ACTION_INCREASE_SUBTITLE_DELAY;
    if(str_action == "EXIT")
        return ACTION_EXIT;
    if(str_action == "PAUSE")
        return ACTION_PLAYPAUSE;
    if(str_action == "DECREASE_VOLUME")
        return ACTION_DECREASE_VOLUME;
    if(str_action == "INCREASE_VOLUME")
        return ACTION_INCREASE_VOLUME;
    if(str_action == "SEEK_BACK_SMALL")
       return ACTION_SEEK_BACK_SMALL;
    if(str_action == "SEEK_FORWARD_SMALL")
        return ACTION_SEEK_FORWARD_SMALL;
    if(str_action == "SEEK_BACK_LARGE")
        return ACTION_SEEK_BACK_LARGE;
    if(str_action == "SEEK_FORWARD_LARGE")
        return ACTION_SEEK_FORWARD_LARGE;
    if(str_action == "STEP")
        return ACTION_STEP;
    if(str_action == "SHOW_SUBTITLES")
        return ACTION_SHOW_SUBTITLES;
    if(str_action == "HIDE_SUBTITLES")
        return ACTION_HIDE_SUBTITLES;
            
    return -1;
}
/* Parses a line from the config file in the mode 'action:key'. Looks up
the action in the relevant enum array. Returns true on success. */
bool getActionAndKeyFromString(string line, int &int_action, string &key)
{
    string str_action;

    if(line[0] == '#')
         return false;

    unsigned int colonIndex = line.find(":");
    if(colonIndex == string::npos)
        return false;

    str_action = line.substr(0,colonIndex);
    key = line.substr(colonIndex+1);

    int_action = convertStringToAction(str_action);

    if(int_action == -1 || key.size() < 1)
        return false;

    return true;
}

/* Returns a keymap consisting of the default
 *  keybinds specified with the -k option 
 */
void KeyConfig::buildDefaultKeymap(unordered_map<int,int> &keymap)
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
void KeyConfig::parseConfigFile(string &filepath, unordered_map<int, int> &keymap)
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
                int n = strtoul(key.substr(4).c_str(), 0, 0);
                if (n > 0)
                  keymap[n] = key_action;
              }
            }
            else keymap[key[0]] = key_action;
        }
    }
}
