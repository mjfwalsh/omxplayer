/*
 *
 *    Copyright (C) 2020 Michael J. Walsh
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string>
#include <stdio.h>

#include "Subtitle.h"

using namespace std;

Subtitle::Subtitle()
{}

Subtitle::Subtitle(int start, int stop, std::string &text_lines)
: start(start),
  stop(stop)
{
  text.swap(text_lines);
  isImage = false;
}

void Subtitle::assign_image(unsigned char *srcData, int size, uint32_t *p)
{
  if(p) {
    unsigned char palette[4];
    for(int i = 0; i < 4; i++, p++) {
      // merge the most significant four bits of the alpha channel with
      // the least significant four bits (index from the dummy palette in
      //  OMXPlayerSubtitles.cpp)
      palette[i] = (*p >> 24 & 0xf0) | (*p & 0xf);
    }

    image.data.resize(size);
    for(int i = 0; i < size; i++) {
      image.data[i] = palette[(int)srcData[i]];
    }
  } else {
    image.data.assign(srcData, size);
  }
}
