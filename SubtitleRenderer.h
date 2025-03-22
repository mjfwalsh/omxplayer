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

#include <cairo/cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "utils/NoMoveCopy.h"

class CRegExp;
class DispmanxLayer;
class Subtitle;
class OMXSubConfig;

class SubtitleRenderer : NoMoveCopy
{
public:
  SubtitleRenderer(OMXSubConfig *config);

  void setDVDSubtitleLayer(DispmanxLayer *dl);

  ~SubtitleRenderer();

  void prepare(Subtitle &sub);
  void prepare(const std::string &lines);
  void show_next();
  void hide();
  void unprepare();
  void clear();

private:
  DispmanxLayer *subtitleLayer = NULL;
  DispmanxLayer *dvdSubLayer = NULL;

  class SubtitleText : NoMoveCopy
  {
    public:
      std::string text;
      int font;
      unsigned int color;

      cairo_glyph_t *glyphs = NULL;
      int num_glyphs = -1;

      SubtitleText(const char *t, int l, int f, unsigned int c)
      : font(f), color(c)
      {
        text.assign(t, l);
      };

      ~SubtitleText()
      {
        cairo_glyph_free(glyphs);
      }

      SubtitleText(SubtitleText &&o) noexcept
      : text(o.text), font(o.font), color(o.color), glyphs(o.glyphs), num_glyphs(o.num_glyphs)
      {
        o.glyphs = NULL;
      }
  };

  void parse_lines(const char *text, int lines_length);
  void make_subtitle_image(std::vector<std::vector<SubtitleText> > &parsed_lines);
  void make_subtitle_image(Subtitle &sub);
  unsigned int hex2int(const char *hex);

  CRegExp *m_tags;
  CRegExp *m_font_color_html;
  CRegExp *m_font_color_curly;

  void set_font(int *old_font, int new_font);
  void set_color(unsigned int *old_color, unsigned int new_color);

  enum {
    UNSET_FONT = -1,
    NORMAL_FONT,
    ITALIC_FONT,
    BOLD_FONT,
  };

  // use the upper 8 bits for these as user selected colours use the lower 24 bits
  const unsigned int FC_NOT_SET   = 0x10000000;
  const unsigned int FC_OFF_WHITE = 0x20000000;
  const unsigned int FC_BLACK     = 0x30000000;
  const unsigned int FC_GHOST     = 0x40000000;

  unsigned char *m_cairo_image_data = NULL;
  unsigned char *m_bitmap_image_data = NULL;

  // cairo stuff
  cairo_surface_t *m_surface;
  cairo_t *m_cr;

  // fonts
  FT_Library  m_ft_library;

  FT_Face     m_ft_face_normal;
  FT_Face     m_ft_face_italic;
  FT_Face     m_ft_face_bold;

  cairo_scaled_font_t *m_scaled_font[3];

  cairo_pattern_t *m_ghost_box_transparency;
  cairo_pattern_t *m_default_font_color;
  cairo_pattern_t *m_black_font_outline;

  // positional elements
  bool m_centered;
  bool m_ghost_box;
  int m_max_lines;

  // font properties
  int m_padding;
  int m_font_size;
};
