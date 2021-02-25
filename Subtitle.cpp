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

#include "Subtitle.h"
#include "utils/simple_geometry.h"

using namespace std;

Subtitle::Subtitle(bool is_image)
: isImage(is_image)
{
  if(isImage) {
    new(&image.data) basic_string<unsigned char>();
  } else {
    new(&text_lines) vector<string>();
  }
}

Subtitle::Subtitle(int start, int stop, vector<string> &tl)
: start(start),
  stop(stop)
{
  isImage = false;
  new(&text_lines) vector<string>();
  text_lines = move(tl);
}

Subtitle::Subtitle(const Subtitle &old) //copy
{
  start = old.start;
  stop = old.stop;
  isImage = old.isImage;

  if(isImage) {
    new(&image.data) basic_string<unsigned char>();
    image.data = old.image.data;
    image.rect = old.image.rect;
  } else {
    new(&text_lines) vector<string>();
    text_lines = old.text_lines;
  }
}

Subtitle& Subtitle::operator=(const Subtitle &old) // copy assign
{
  start = old.start;
  stop = old.stop;
  isImage = old.isImage;

  if(isImage) {
    new(&image.data) basic_string<unsigned char>();
    image.data = old.image.data;
    image.rect = old.image.rect;
  } else {
    new(&text_lines) vector<string>();
    text_lines = old.text_lines;
  }
  return *this;
}

Subtitle::Subtitle(Subtitle &&old) noexcept //move
{
  start = old.start;
  stop = old.stop;
  isImage = old.isImage;

  if(isImage) {
    new(&image.data) basic_string<unsigned char>();
    image.data = move(old.image.data);
    image.rect = move(old.image.rect);
  } else {
    new(&text_lines) vector<string>();
    text_lines = move(old.text_lines);
  }
}

Subtitle& Subtitle::operator=(Subtitle &&old) noexcept // move assign
{
  start = old.start;
  stop = old.stop;
  isImage = old.isImage;

  if(isImage) {
    new(&image.data) basic_string<unsigned char>();
    image.data = move(old.image.data);
    image.rect = move(old.image.rect);
  } else {
    new(&text_lines) vector<string>();
    text_lines = move(old.text_lines);
  }
  return *this;
}

Subtitle::~Subtitle()
{
  if(isImage) {
    image.data.~basic_string<unsigned char>();
  } else {
    text_lines.~vector<string>();
  }
}
