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
{
  refcount = NULL;
  if(isImage) {
    image.data = NULL;
  } else {
    text.lines = NULL;
  }
}

Subtitle::Subtitle(int start, int stop, const char *t, int l)
: start(start),
  stop(stop)
{
  isImage = false;
  refcount = new int;
  *refcount = 1;

  text.length = l;
  text.lines = new char[l + 1];
  memcpy(text.lines, t, l);
  text.lines[l] = '\0';
}

void Subtitle::assign_image(unsigned char *srcData, unsigned char *palette, int size)
{
  refcount = new int;
  *refcount = 1;

  image.data = new unsigned char[size];

  for(int i = 0; i < size; i++) {    
    int x = (int)srcData[i];
    
    assert(x >= 0 && x <= 3);

    image.data[i] = palette[x];
  }
}

void Subtitle::alloc_text(int size)
{
  refcount = new int;
  *refcount = 1;

  text.lines = new char[size + 1];
}

Subtitle::Subtitle(const Subtitle &old) //copy
{
  start = old.start;
  stop = old.stop;
  isImage = old.isImage;

  if(isImage) {
    image.data = old.image.data;
    image.rect = old.image.rect;
  } else {
    text.lines = old.text.lines;
    text.length = old.text.length;
  }

  refcount = old.refcount;
  (*refcount)++;
}

Subtitle& Subtitle::operator=(const Subtitle &old) // copy assign
{
  start = old.start;
  stop = old.stop;
  isImage = old.isImage;

  if(isImage) {
    image.data = old.image.data;
    image.rect = old.image.rect;

  } else {
    text.lines = old.text.lines;
    text.length = old.text.length;
  }

  refcount = old.refcount;
  (*refcount)++;
  return *this;
}

Subtitle::Subtitle(Subtitle &&old) noexcept //move
{
  start = old.start;
  stop = old.stop;
  isImage = old.isImage;
  refcount = old.refcount;

  old.refcount = NULL;

  if(isImage) {
    image.data = old.image.data;
    image.rect = move(old.image.rect);
    old.image.data = NULL;
  } else {
    text.lines = old.text.lines;
    text.length = old.text.length;
    old.text.lines = NULL;
  }
}

Subtitle& Subtitle::operator=(Subtitle &&old) noexcept // move assign
{
  start = old.start;
  stop = old.stop;
  isImage = old.isImage;
  refcount = old.refcount;

  old.refcount = NULL;

  if(isImage) {
    image.data = old.image.data;
    image.rect = move(old.image.rect);
    old.image.data = NULL;
  } else {
    text.lines = old.text.lines;
    text.length = old.text.length;
    old.text.lines = NULL;
  }
  return *this;
}

Subtitle::~Subtitle()
{
  if(refcount == NULL)
    return;

  if(isImage) {
    if(*refcount == 1) {
      delete image.data;
      delete refcount;
    } else if(*refcount > 1) {
      (*refcount)--;
    }
  } else {
    if(*refcount == 1) {
      delete text.lines;
      delete refcount;
    } else if(*refcount > 1) {
      (*refcount)--;
    }
  }
}
