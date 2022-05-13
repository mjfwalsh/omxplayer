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


#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <cairo.h>

#include "utils/RegExp.h"
#include "SubtitleRenderer.h"
#include "DispmanxLayer.h"
#include "Subtitle.h"

using namespace std;

SubtitleRenderer::SubtitleRenderer(int display_num, int layer_num, float r_font_size,
	bool centered, bool box_opacity, unsigned int lines)
: m_centered(centered),
  m_ghost_box(box_opacity),
  m_max_lines(lines)
{
	// Subtitle tag parser regexes
	m_tags = new CRegExp("(\\n|<[^>]*>|\\{\\\\[^\\}]*\\})");
	m_font_color_html = new CRegExp("color[ \\t]*=[ \\t\"']*#?([a-f0-9]{6})");
	m_font_color_curly = new CRegExp("^\\{\\\\c&h([a-f0-9]{2})([a-f0-9]{2})([a-f0-9]{2})&\\}$");

	// Open display
	DispmanxLayer::openDisplay(display_num, layer_num);

	// Determine screen size
	Dimension screen = DispmanxLayer::getScreenDimensions();

	/*    *    *    *     *    *    *    *    *    *    *    *
	 * Set up layer for text subtitles and on screen display *
	 *    *    *    *     *    *    *    *    *    *    *    */

	//Calculate font as thousands of screen height
	m_font_size = screen.height * r_font_size;

	// Calculate padding as 1/4 of the font size
	m_padding = m_font_size / 4;

	// And line_height combines the two
	int line_height = m_font_size + m_padding;

	// A fairly unscientific survey showed that with a font size of 59px subtitles lines
	// were rarely longer than 1300px. We also assume that marger font sizes (frequently used
	// in East Asian scripts) would result in shorter not longer subtitles.
	int assumed_longest_subtitle_line_in_pixels = 1300;

	// Calculate image dimensions - must be evenly divisible by 16
	Rect text_subtitle_rect;
	text_subtitle_rect.height = (m_max_lines * line_height + 20) & ~15; // grow to fit
	text_subtitle_rect.y = screen.height - text_subtitle_rect.height - (line_height / 2);

	if(m_centered) {
		text_subtitle_rect.width = (screen.width - 100) & ~15; // shrink to fit
		text_subtitle_rect.x = (screen.width - text_subtitle_rect.width) / 2; // centered on screen
	} else {
		if(screen.width > assumed_longest_subtitle_line_in_pixels) {
			text_subtitle_rect.x = (screen.width - assumed_longest_subtitle_line_in_pixels) / 2;
		} else if(screen.width > screen.height) {
			text_subtitle_rect.x = (screen.width - screen.height) / 2;
		} else {
			text_subtitle_rect.x = 100;
		}

		text_subtitle_rect.width = (screen.width - 100 - text_subtitle_rect.x) & ~15; // shrink to fit
	}

	// Create layer
	subtitleLayer = new DispmanxLayer(4, text_subtitle_rect);

	// font faces
	cairo_font_face_t *normal_font = cairo_toy_font_face_create("FreeSans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_font_face_t *italic_font = cairo_toy_font_face_create("FreeSans", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_font_face_t *bold_font = cairo_toy_font_face_create("FreeSans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

	// prepare scaled fonts
    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, m_font_size, m_font_size);
    cairo_font_options_t *options = cairo_font_options_create();

	m_normal_font_scaled = cairo_scaled_font_create(normal_font, &sizeMatrix, &ctm, options);
	m_italic_font_scaled = cairo_scaled_font_create(italic_font, &sizeMatrix, &ctm, options);
	m_bold_font_scaled = cairo_scaled_font_create(bold_font, &sizeMatrix, &ctm, options);

	// font colours
	m_ghost_box_transparency = cairo_pattern_create_rgba(0, 0, 0, 0.5f);
	m_default_font_color = cairo_pattern_create_rgba(0.866667, 0.866667, 0.866667, 1);
	m_black_font_outline = cairo_pattern_create_rgba(0, 0, 0, 1);

	// cleanup
	cairo_font_options_destroy(options);
	cairo_font_face_destroy(normal_font);
	cairo_font_face_destroy(italic_font);
	cairo_font_face_destroy(bold_font);
}
	/*    *    *    *    *    *    *    *    *    *    *    *
	 *            Set up layer for DVD subtitles            *
	 *    *    *    *    *    *    *    *    *    *    *    */

void SubtitleRenderer::initDVDSubs(Dimension video, float video_aspect_ratio, int aspect_mode,
		uint32_t *palette)
{
	if(dvdSubLayer)
		delete dvdSubLayer;

	// Determine screen size
	Dimension screen = DispmanxLayer::getScreenDimensions();

	// Calculate position of view port
	Rect view_port(0, 0, screen.width, screen.height);
	float screen_aspect_ratio = (float)screen.width / screen.height;
	if((aspect_mode == 1 || aspect_mode == 0) && video_aspect_ratio != screen_aspect_ratio) {
		if(video_aspect_ratio > screen_aspect_ratio) {
			view_port.height = screen.width * video_aspect_ratio;
		} else {
			view_port.width = screen.height * video_aspect_ratio;
		}
	}

	// adjust width and height so they are divisible by 16
	view_port.width = (view_port.width + 8) & ~15;
	view_port.height = (view_port.height + 8) & ~15;
	view_port.x = (screen.width - view_port.width) / 2;
	view_port.y = (screen.height - view_port.height) / 2;

	// create layer
	dvdSubLayer = new DispmanxLayer(1, view_port, video, palette);
}

void SubtitleRenderer::set_font(int new_font_type)
{
	if(new_font_type == m_current_font) return;

	switch(new_font_type) {
		case NORMAL_FONT:
			cairo_set_scaled_font(m_cr, m_normal_font_scaled);
			break;
		case BOLD_FONT:
			cairo_set_scaled_font(m_cr, m_bold_font_scaled);
			break;
		case ITALIC_FONT:
			cairo_set_scaled_font(m_cr, m_italic_font_scaled);
			break;
	}

	m_current_font = new_font_type;
}

void SubtitleRenderer::set_color(int new_color)
{
	if(new_color == m_color) return;

	if(new_color == -1)
		cairo_set_source(m_cr, m_default_font_color);
	else if(new_color == -2)
		cairo_set_source(m_cr, m_ghost_box_transparency);
	else if(new_color == 0)
		cairo_set_source(m_cr, m_black_font_outline);
	else {
		float r = ((new_color >> 16) & 0xFF) / 255.0f;
		float g = ((new_color >>  8) & 0xFF) / 255.0f;
		float b = ((new_color >>  0) & 0xFF) / 255.0f;

		cairo_set_source_rgba(m_cr, r, g, b, 1);
	}

	m_color = new_color;
}


void SubtitleRenderer::prepare(Subtitle &sub)
{
	unprepare();

	if(sub.isImage)
		make_subtitle_image(sub);
	else
		parse_lines(sub.text.data(), sub.text.length());
}

void SubtitleRenderer::prepare(string &lines)
{
    unprepare();

    parse_lines(lines.data(), lines.length());
}

void SubtitleRenderer::make_subtitle_image(vector<vector<SubtitleText> > &parsed_lines)
{
	// create surface
	m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, subtitleLayer->getSourceWidth(), subtitleLayer->getSourceHeight());
	m_cr = cairo_create(m_surface);

	// Reset font control vars as no font or drawing dolour has been set
	m_current_font = -500;
	m_color = -500;

	// cursor y position
	int cursor_y_position = subtitleLayer->getSourceHeight() - m_padding;

	// Limit the number of line
	int no_of_lines = parsed_lines.size();
	if(no_of_lines > m_max_lines) no_of_lines = m_max_lines;

	for(int i = no_of_lines - 1; i > -1; i--) {
		int box_width = (m_padding * 2);
		int text_parts = parsed_lines[i].size();

		// cursor x position
		int cursor_x_position = 0;

		for(int j = 0; j < text_parts; j++) {
			set_font(parsed_lines[i][j].font);

			// prepare font glyphs
			cairo_status_t status = cairo_scaled_font_text_to_glyphs(
					cairo_get_scaled_font(m_cr),
					cursor_x_position + m_padding,
					cursor_y_position - (m_padding / 4),
					parsed_lines[i][j].text.data(),
					parsed_lines[i][j].text.length(),
					&parsed_lines[i][j].glyphs,
					&parsed_lines[i][j].num_glyphs,
					NULL, NULL, NULL);

			if (status != CAIRO_STATUS_SUCCESS)
				return;

			// calculate font extents
			cairo_text_extents_t extents;
			cairo_glyph_extents (m_cr,
				parsed_lines[i][j].glyphs,
				parsed_lines[i][j].num_glyphs,
				&extents);

			cursor_x_position += extents.x_advance;
			box_width += extents.x_advance;
		}

		// aligned text
		if(m_centered) {
			cursor_x_position = (subtitleLayer->getSourceWidth() / 2) - (box_width / 2);

			for(int j = 0; j < text_parts; j++) {
				cairo_glyph_t *p = parsed_lines[i][j].glyphs;
				for(int h = 0; h < parsed_lines[i][j].num_glyphs; h++, p++) {
					p->x +=cursor_x_position;
				}
			}
		} else {
			cursor_x_position = 0;
		}

		// draw ghost box
		if(m_ghost_box) {
			set_color(-2);
			cairo_rectangle(m_cr, cursor_x_position, cursor_y_position - m_font_size, box_width,
				m_font_size + m_padding);
			cairo_fill(m_cr);
		}

		for(int j = 0; j < text_parts; j++) {
			set_font(parsed_lines[i][j].font);
			set_color(parsed_lines[i][j].color);

			// draw text
			cairo_glyph_path(m_cr, parsed_lines[i][j].glyphs, parsed_lines[i][j].num_glyphs);

			// free glyph array
			cairo_glyph_free(parsed_lines[i][j].glyphs);
		}

		// draw black text outline
		cairo_fill_preserve(m_cr);
		set_color(0);
		cairo_set_line_width(m_cr, 2);
		cairo_stroke(m_cr);

		// next line
		cursor_y_position -= m_font_size + m_padding;
	}

	cairo_image_data = cairo_image_surface_get_data(m_surface);
	m_prepared_from_text = true;
}


void SubtitleRenderer::make_subtitle_image(Subtitle &sub)
{
	unsigned char *p;

	// Subtitles which exceed dimensions are ignored
	if(sub.image.rect.x + sub.image.rect.width  > dvdSubLayer->getSourceWidth() || sub.image.rect.y + sub.image.rect.height  > dvdSubLayer->getSourceHeight())
	  return;

	p = other_image_data = (unsigned char *)malloc(dvdSubLayer->getSourceWidth() * dvdSubLayer->getSourceHeight());

	auto mem_set = [&p](int num_pixels)
	{
		memset(p, '\0', num_pixels);
		p += num_pixels;
	};

	auto mem_copy = [&p](const unsigned char *pixel, int len)
	{
		memcpy(p, pixel, len);
		p += len;
	};

	int right_padding  = dvdSubLayer->getSourceWidth()  - sub.image.rect.width  - sub.image.rect.x;
	int bottom_padding = dvdSubLayer->getSourceHeight() - sub.image.rect.height - sub.image.rect.y;

	// blanks char at top
	mem_set(sub.image.rect.y * dvdSubLayer->getSourceWidth());

	for(int j = 0; j < sub.image.rect.height; j++) {
		mem_set(sub.image.rect.x);
		mem_copy(sub.image.data.data() + (j * sub.image.rect.width), sub.image.rect.width);
		mem_set(right_padding);
	}

	// blanks char at bottom
	mem_set(bottom_padding * dvdSubLayer->getSourceWidth());

	m_prepared_from_image = true;
}

void SubtitleRenderer::show_next()
{
	if(m_prepared_from_image) {
		subtitleLayer->hideElement();
		dvdSubLayer->setImageData(other_image_data);
		unprepare();
	} else if(m_prepared_from_text) {
		if(dvdSubLayer) dvdSubLayer->hideElement();
		subtitleLayer->setImageData(cairo_image_data);
		unprepare();
	}
}

void SubtitleRenderer::hide()
{
	subtitleLayer->hideElement();
	if(dvdSubLayer) dvdSubLayer->hideElement();
}

void SubtitleRenderer::clear()
{
	subtitleLayer->clearImage();
	if(dvdSubLayer) dvdSubLayer->clearImage();
}

void SubtitleRenderer::unprepare()
{
	if(m_prepared_from_image) {
		free(other_image_data);
		m_prepared_from_image = false;
	}

	if(m_prepared_from_text) {
		cairo_destroy(m_cr);
		cairo_surface_destroy(m_surface);
		m_prepared_from_text = false;
	}
}

// Tag parser functions
void SubtitleRenderer::parse_lines(const char *text, int lines_length)
{
	vector<vector<SubtitleText> > formatted_lines(1);

	bool bold = false, italic = false;
	int color = -1;

    int pos = 0, old_pos = 0;
    while (pos < lines_length) {
        pos = m_tags->RegFind(text, pos, lines_length);
        string fullTag = m_tags->GetMatch(0);

        //parse text
        if(pos != old_pos || fullTag == "\n") {
            int l = pos == -1 ? lines_length - old_pos : pos - old_pos;

            if(l > 0) {
                int font = italic ? ITALIC_FONT : (bold ? BOLD_FONT : NORMAL_FONT);
                formatted_lines.back().emplace_back(text + old_pos, l, font, color);
            }
        }

        // No more tags found
        if(pos < 0) break;

        // Parse Tag
        boost::algorithm::to_lower(fullTag);
        pos += fullTag.length();
        old_pos = pos;

        if (fullTag == "\n" ) {
            formatted_lines.emplace_back();
        } else if (fullTag == "<b>" || fullTag == "{\\b1}") {
            bold = true;
        } else if ((fullTag == "</b>" || fullTag == "{\\b0}") && bold) {
            bold = false;
        } else if (fullTag == "<i>" || fullTag == "{\\i1}") {
            italic = true;
        } else if ((fullTag == "</i>" || fullTag == "{\\i0}") && italic) {
            italic = false;
        } else if ((fullTag == "</font>" || fullTag == "{\\c}") && color != -1) {
            color = -1;
        } else if (fullTag.substr(0,5) == "<font") {
            if(m_font_color_html->RegFind(fullTag.c_str(), 5) >= 0) {
                color = hex2int(m_font_color_html->GetMatch(1).c_str());
            }
        } else if(m_font_color_curly->RegFind(fullTag.c_str(), 0) >= 0) {
            string t = m_font_color_curly->GetMatch(3) + m_font_color_curly->GetMatch(2)
                + m_font_color_curly->GetMatch(1);
            color = hex2int(t.c_str());
        }
    }

	make_subtitle_image(formatted_lines);
}

// expects 6 lowercase, digit hex string
int SubtitleRenderer::hex2int(const char *hex)
{
	int r = 0;
	for(int i = 0, f = 20; i < 6; i++, f -= 4)
		if(hex[i] >= 'a')
			r += (hex[i] - 87) << f;
		else
			r += (hex[i] - 48) << f;

	return r;
}

SubtitleRenderer::~SubtitleRenderer()
{
	//destroy cairo surface, if defined
	unprepare();

	// remove DispmanX layer
	delete subtitleLayer;
	if(dvdSubLayer) delete dvdSubLayer;
	DispmanxLayer::closeDisplay();

	// destroy cairo fonts
	cairo_scaled_font_destroy(m_normal_font_scaled);
	cairo_scaled_font_destroy(m_italic_font_scaled);
	cairo_scaled_font_destroy(m_bold_font_scaled);

	//delete regexes
	delete m_tags;
	delete m_font_color_html;
	delete m_font_color_curly;
}
