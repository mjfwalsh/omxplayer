#include <unordered_map>

class KeyConfig
{

  public: 
    enum 
    {
        // These enums are used by the Action dbus method so the
        // numbers should be maintained for compatibility purposes
        ACTION_DECREASE_SPEED = 1,
        ACTION_INCREASE_SPEED = 2,
        //ACTION_REWIND = 3,
        //ACTION_FAST_FORWARD = 4,
        //ACTION_SHOW_INFO = 5,
        ACTION_PREVIOUS_AUDIO = 6,
        ACTION_NEXT_AUDIO = 7,
        ACTION_PREVIOUS_CHAPTER = 8,
        ACTION_NEXT_CHAPTER = 9,
        ACTION_PREVIOUS_SUBTITLE = 10,
        ACTION_NEXT_SUBTITLE = 11,
        ACTION_TOGGLE_SUBTITLE = 12,
        ACTION_DECREASE_SUBTITLE_DELAY = 13,
        ACTION_INCREASE_SUBTITLE_DELAY = 14,
        ACTION_EXIT = 15,
        ACTION_PLAYPAUSE = 16,
        ACTION_DECREASE_VOLUME = 17,
        ACTION_INCREASE_VOLUME = 18,
        ACTION_SEEK_BACK_SMALL = 19,
        ACTION_SEEK_FORWARD_SMALL = 20,
        ACTION_SEEK_BACK_LARGE = 21,
        ACTION_SEEK_FORWARD_LARGE = 22,
        ACTION_STEP = 23,
        //ACTION_BLANK = 24,
        ACTION_SEEK_RELATIVE = 25,
        //ACTION_SEEK_ABSOLUTE = 26, (needs param)
        //ACTION_MOVE_VIDEO = 27, (needs param)
        ACTION_HIDE_VIDEO = 28,
        ACTION_UNHIDE_VIDEO = 29,
        ACTION_HIDE_SUBTITLES = 30,
        ACTION_SHOW_SUBTITLES = 31,
        //ACTION_SET_ALPHA = 32, (needs param)
        //ACTION_SET_ASPECT_MODE = 33, (needs param)
        //ACTION_CROP_VIDEO = 34, (needs param)
        ACTION_PAUSE = 35,
        ACTION_PLAY = 36,
        //ACTION_CHANGE_FILE = 37,
        //ACTION_SET_LAYER = 38, (needs param)

        ACTION_PREVIOUS_FILE = 39,
        ACTION_NEXT_FILE = 40,

        // From here on the methods require parameters
        // so they can't work with the Action method
        ACTION_BLANK = 100,
        ACTION_SEEK_ABSOLUTE,
        ACTION_SET_ALPHA,
        ACTION_SET_ASPECT_MODE,
        ACTION_CHANGE_FILE,
        ACTION_SET_LAYER,
        ACTION_SET_VOLUME,
        ACTION_SET_SPEED,
        ACTION_UPDATE_SUBTITLES,
        ACTION_UPDATE_AUDIO,
    };

    #define KEY_LEFT 0x5b44
    #define KEY_RIGHT 0x5b43
    #define KEY_UP 0x5b41
    #define KEY_DOWN 0x5b42
    #define KEY_ESC 27

    static void buildDefaultKeymap(std::unordered_map<int, int> &keymap);
    static void parseConfigFile(std::string &filepath, std::unordered_map<int, int> &keymap);
};
