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

#include <cairo.h>

#include "utils/simple_geometry.h"

class CRegExp;
class DispmanxLayer;
class Subtitle;

using namespace std;

class SubtitleRenderer {
	public:
		SubtitleRenderer(const SubtitleRenderer&) = delete;
		SubtitleRenderer& operator=(const SubtitleRenderer&) = delete;
		SubtitleRenderer(int display, int layer,
						float r_font_size,
						bool centered,
						bool box_opacity,
						unsigned int lines);

		void initDVDSubs(Dimension video, float video_aspect_ratio,
				int aspect_mode);

		~SubtitleRenderer();

		void prepare(Subtitle &sub);
		void prepare(vector<string> &lines);
		void show_next();
		void hide();
		void unprepare();
		void clear();

	private:
		DispmanxLayer *subtitleLayer;
		DispmanxLayer *dvdSubLayer;

		class SubtitleText
		{
			public:
				string text;
				int font;
				int color;

				cairo_glyph_t *glyphs = NULL;
				int num_glyphs = -1;

				SubtitleText(string t, int f, int c)
				: text(t), font(f), color(c)
				{
				};
		};

		void parse_lines(vector<string> &text_lines);
		void make_subtitle_image(vector<vector<SubtitleText> > &parsed_lines);
		void make_subtitle_image(Subtitle &sub);
		int hex2int(const char *hex);

		CRegExp *m_tags;
		CRegExp *m_font_color_html;
		CRegExp *m_font_color_curly;

		void set_font(int new_font_type);
		void set_color(int new_color);

		enum {
			NORMAL_FONT,
			ITALIC_FONT,
			BOLD_FONT,
		};

		bool m_prepared_from_image = false;
		bool m_prepared_from_text = false;
		unsigned char *cairo_image_data;
		unsigned char *other_image_data;

		// cairo stuff
		cairo_surface_t *m_surface;
		cairo_t *m_cr;

		// fonts
		cairo_scaled_font_t *m_normal_font_scaled;
		cairo_scaled_font_t *m_italic_font_scaled;
		cairo_scaled_font_t *m_bold_font_scaled;

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
		int m_current_font;
		int m_color;
};
