/*
 * straccum.{cc,hh} -- build up strings with operator<<
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "straccum.hh"
#include "string.hh"
#include "confparse.hh"
#include "glue.hh"

void
StringAccum::grow(int want)
{
  int ocap = _cap;
  _cap = (_cap ? _cap * 2 : 128);
  while (_cap <= want)
    _cap *= 2;
  
  unsigned char *n = new unsigned char[_cap];
  if (_s) memcpy(n, _s, ocap);
  delete[] _s;
  _s = n;
}

String
StringAccum::take_string()
{
  int len = length();
  return String::claim_string(take(), len);
}

StringAccum &
StringAccum::operator<<(const char *s)
{
  int len = strlen(s);
  memcpy(extend(len), s, len);
  return *this;
}

#ifdef HAVE_PERMSTRING
StringAccum &
StringAccum::operator<<(PermString s)
{
  memcpy(extend(s.length()), s.cc(), s.length());
  return *this;
}
#endif

StringAccum &
StringAccum::operator<<(const String &s)
{
  memcpy(extend(s.length()), s.data(), s.length());
  return *this;
}

StringAccum &
StringAccum::operator<<(int i)
{
  int len;
  sprintf(reserve(256), "%d%n", i, &len);
  forward(len);
  return *this;
}

StringAccum &
StringAccum::operator<<(unsigned u)
{
  int len;
  sprintf(reserve(256), "%u%n", u, &len);
  forward(len);
  return *this;
}

#ifndef __KERNEL__
StringAccum &
StringAccum::operator<<(double d)
{
  int len;
  sprintf(reserve(256), "%g%n", d, &len);
  forward(len);
  return *this;
}
#endif

StringAccum &
StringAccum::operator<<(unsigned long long q)
{
#ifndef CLICK_TOOL
  String qstr = cp_unparse_ulonglong(q, 10, false);
  memcpy(extend(qstr.length()), qstr.data(), qstr.length());
#else
  int len;
  sprintf(reserve(256), "%qu%n", q, &len);
  forward(len);
#endif
  return *this;
}
