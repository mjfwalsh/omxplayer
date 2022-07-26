#pragma once
/*
 * Copyright (C) 2022 by Michael J. Walsh
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdnav; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

class Rect
{
public:
	Rect() : x(0), y(0), width(0), height(0) {}
	Rect(int xp, int yp, int w, int h) : x(xp), y(yp), width(w), height(h) {}

	int x, y, width, height;
};

class Dimension
{
public:
	Dimension() : width(0), height(0) {}
	Dimension(int w, int h) : width(w), height(h) {}
	
	int width, height;
};
