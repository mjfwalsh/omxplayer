/*
 *
 *		Copyright (C) 2020 Michael J. Walsh
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

#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <assert.h>

#include "Subtitle.h"
#include "utils/simple_geometry.h"

using namespace std;

Subtitle::Subtitle(bool is_image)
: isImage(is_image)
{}

Subtitle::Subtitle(int start, int stop, std::string &text_lines)
: start(start),
  stop(stop)
{
  text.swap(text_lines);
  isImage = false;
}

void Subtitle::assign_image(unsigned char *srcData, int size, unsigned char *palette)
{
  image.data = new unsigned char[size];

  if(palette == NULL) {
    image.data.assign(srcData, size);
  } else {
    image.data.resize(size);
    for(int i = 0; i < size; i++) {
      image.data[i] = palette[(int)srcData[i]];
    }
  }
}

