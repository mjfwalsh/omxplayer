/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#ifdef __GNUC__
// under gcc, inline will only take place if optimizations are applied (-O). this will force inline even whith optimizations.
#define XBMC_FORCE_INLINE __attribute__((always_inline))
#else
#define XBMC_FORCE_INLINE
#endif


class CRect
{
public:
  CRect() { x1 = y1 = x2 = y2 = 0;};
  CRect(int left, int top, int right, int bottom) { x1 = left; y1 = top; x2 = right; y2 = bottom; };

  void SetRect(int left, int top, int right, int bottom) { x1 = left; y1 = top; x2 = right; y2 = bottom; };

  inline int Width() const XBMC_FORCE_INLINE
  {
    return x2 - x1;
  };

  inline int Height() const XBMC_FORCE_INLINE
  {
    return y2 - y1;
  };

  int x1, y1, x2, y2;
};

