#pragma once
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

#include <string>

namespace PCRE {
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
}

class CRegExp
{
public:
  explicit CRegExp(const char *re, bool casesensitive = false);

  ~CRegExp();

  int RegFind(const char *str, int startoffset = 0, int len = -1);
  inline int RegFind(const std::string& str, int startoffset = 0) { return RegFind(str.c_str(), startoffset, str.length()); }
  inline int RegFind(const std::string& str, int startoffset, int len) { return RegFind(str.c_str(), startoffset, len); }
  std::string GetMatch(int iSub = 0);

private:
  PCRE::pcre2_code *m_re;
  size_t *m_iOvector = NULL;
  PCRE::pcre2_match_data *m_match_data;
  int         m_iMatchCount = 0;
  int         m_iOptions = PCRE2_DOTALL;
  bool        m_bMatched = false;
  std::string m_subject;
  std::string m_pattern;
};
