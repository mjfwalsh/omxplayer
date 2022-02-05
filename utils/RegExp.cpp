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
#include <assert.h>
#include "RegExp.h"
#include "log.h"

using namespace PCRE;

CRegExp::CRegExp(const char *re, bool casesensitive /* = false */)
{
  if(!casesensitive)
    m_iOptions |= PCRE_CASELESS;

  const char *errMsg = NULL;
  int errOffset      = 0;

  m_re = pcre_compile(re, m_iOptions, &errMsg, &errOffset, NULL);
  if (!m_re)
  {
    printf("PCRE: %s. Compilation failed at offset %d in expression '%s'\n",
              errMsg, errOffset, re);
    assert(0);
  }
}


CRegExp::~CRegExp()
{
  PCRE::pcre_free(m_re);
}

int CRegExp::RegFind(const char* str, int startoffset, int str_len /*= -1*/)
{
  m_bMatched    = false;
  m_iMatchCount = 0;

  if (!str)
  {
    CLogLog(LOGERROR, "PCRE: Called without a string to match");
    return -1;
  }

  if(str_len == -1)
    str_len = strlen(str);

  m_subject = str;
  int rc = pcre_exec(m_re, NULL, str, str_len, startoffset, 0, m_iOvector, OVECCOUNT);

  if (rc<1)
  {
    switch(rc)
    {
    case PCRE_ERROR_NOMATCH:
      return -1;

    case PCRE_ERROR_MATCHLIMIT:
      CLogLog(LOGERROR, "PCRE: Match limit reached");
      return -1;

    default:
      CLogLog(LOGERROR, "PCRE: Unknown error: %d", rc);
      return -1;
    }
  }
  m_bMatched = true;
  m_iMatchCount = rc;
  return m_iOvector[0];
}

int CRegExp::GetCaptureTotal()
{
  int c = -1;
  pcre_fullinfo(m_re, NULL, PCRE_INFO_CAPTURECOUNT, &c);
  return c;
}

std::string CRegExp::GetMatch(int iSub /* = 0 */)
{
  if (iSub < 0 || iSub >= m_iMatchCount)
    return "";

  int pos = m_iOvector[(iSub*2)];
  int len = m_iOvector[(iSub*2)+1] - pos;
  return m_subject.substr(pos, len);
}
