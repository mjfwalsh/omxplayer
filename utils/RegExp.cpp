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
#include <string.h>

#include "RegExp.h"
#include "log.h"

using namespace PCRE;

CRegExp::CRegExp(const char *re, bool casesensitive /* = false */)
{
  if(!casesensitive)
    m_iOptions |= PCRE2_CASELESS;

  int errCode;
  PCRE2_SIZE errOffset;

  m_re = pcre2_compile((const unsigned char *)re, PCRE2_ZERO_TERMINATED, m_iOptions, &errCode, &errOffset, nullptr);

  m_match_data = pcre2_match_data_create_from_pattern(m_re, nullptr);

  if (!m_re)
  {
    printf("PCRE: Compilation failed for expression '%s'\n", re);
    throw "PCRE: Compilation failed";
  }
}


CRegExp::~CRegExp()
{
  pcre2_code_free(m_re);
  pcre2_match_data_free(m_match_data);
}

int CRegExp::RegFind(const std::string &str, int startoffset)
{
  m_bMatched    = false;
  m_iMatchCount = 0;

  m_subject = str;
  int rc = pcre2_match(m_re, (const unsigned char *)str.c_str(), str.size(), startoffset, 0, m_match_data, nullptr);

  if (rc < 1)
  {
    if(rc != PCRE2_ERROR_NOMATCH)
      CLogLog(LOGERROR, "PCRE: Error: %d", rc);

    return -1;
  }

  m_bMatched = true;
  m_iMatchCount = rc;

  m_iOvector = pcre2_get_ovector_pointer(m_match_data);

  return m_iOvector[0];
}

std::string CRegExp::GetMatch(int iSub /* = 0 */)
{
  if (iSub < 0 || iSub >= m_iMatchCount)
    return "";

  int pos = m_iOvector[(iSub*2)];
  int len = m_iOvector[(iSub*2)+1] - pos;
  return m_subject.substr(pos, len);
}
