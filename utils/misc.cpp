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
#include "RegExp.h"

// reads input from r
// writes a single char output to w
// returns number of input chars processed
static int uri_unescape_helper(const char *r, char *w)
{
    if(*r == '%') {
        // read '%%' write '%'
        *w = '%';
        return 2;
    }

    int out[2];
    for(int i = 0; i < 2; i++) {
        switch(*r) {
        case 'A' ... 'F':
            out[i] = (int)*r - 'A' + 10;
            break;
        case 'a' ... 'f':
            out[i] = (int)*r - 'a' + 10;
            break;
        case '0' ... '9':
            out[i] = (int)*r - '0';
            break;
        default:
            *w = '%';
            return 1;
        }
        r++;
    }

    int result = (out[0] << 4) | out[1];
    if(result < 32 || result == 127) {
        // ignore control chars
        *w = ' ';
        return 3;
    } else {
        *w = (char)result;
        return 3;
    }
}

void uri_unescape(std::string &in)
{
    char *r = &in[0];
    char *w = &in[0];
    while(*r != '\0') {
        if(*r == '%') {
            r += uri_unescape_helper(r+1, w);
            w++;
        } else {
            *w++ = *r++;
        }
    }
    in.resize(w - &in[0]);
}

// Check file exists and is readable
bool Exists(const std::string& path)
{
  FILE *file = fopen(path.c_str(), "r");
  if(file)
  {
    fclose(file);
    return true;
  }
  return false;
}

bool IsURL(const std::string &str)
{
  static CRegExp protocol_match("^[a-zA-Z]+://");
  return protocol_match.RegFind(str, 0) > -1;
}

bool IsPipe(const std::string& str)
{
  return str.substr(0, 5) == "pipe:";
}
