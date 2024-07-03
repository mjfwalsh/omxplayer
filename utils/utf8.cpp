#include <string>

// http://www.zedwood.com/article/cpp-is-valid-utf8-string-function
static bool isValidUtf8(const std::string &string)
{
    for(int i = 0, ix = string.length(); i < ix; i++)
    {
        int n = 0;
        int c = (unsigned char) string[i];
        if (0x00 <= c && c <= 0x7f) n = 0;
        else if((c & 0xE0) == 0xC0) n = 1;
        else if(c == 0xed && i < (ix - 1) && ((unsigned char)string[i+1] & 0xa0) == 0xa0) return false;
        else if((c & 0xF0) == 0xE0) n = 2;
        else if((c & 0xF8) == 0xF0) n = 3;
        else return false;

        for (int j = 0; j < n && i < ix; j++) {
            if ((++i == ix) || (((unsigned char)string[i] & 0xC0) != 0x80))
                return false;
        }
    }
    return true;
}


void checkAndRescueUtf8Strings(std::string &string)
{
    // if it's valid utf8, do nothing
    if(isValidUtf8(string))
      return;

    // otherwise rescue string by removing non-ascii chars
    for(auto &c : string)
    {
        if(c < '\x00' || c > '\x7f')
            c = '?';
    }
}
