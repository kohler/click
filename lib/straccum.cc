/*
 * straccum.{cc,hh} -- build up strings with operator<<
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
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

bool
StringAccum::grow(int want)
{
  int ncap = (_cap ? _cap * 2 : 128);
  while (ncap <= want)
    ncap *= 2;
  
  unsigned char *n = new unsigned char[ncap];
  if (!n)
    return false;
  
  if (_s)
    memcpy(n, _s, _cap);
  delete[] _s;
  _s = n;
  _cap = ncap;
  return true;
}

const char *
StringAccum::cc()
{
  push('\0');
  pop();
  return reinterpret_cast<char *>(_s);
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
  push(s, strlen(s));
  return *this;
}

#ifdef HAVE_PERMSTRING
StringAccum &
StringAccum::operator<<(PermString s)
{
  push(s.cc(), s.length());
  return *this;
}
#endif

StringAccum &
StringAccum::operator<<(const String &s)
{
  push(s.data(), s.length());
  return *this;
}

StringAccum &
StringAccum::operator<<(int i)
{
  if (char *x = reserve(256)) {
    int len;
    sprintf(x, "%d%n", i, &len);
    forward(len);
  }
  return *this;
}

StringAccum &
StringAccum::operator<<(unsigned u)
{
  if (char *x = reserve(256)) {
    int len;
    sprintf(x, "%u%n", u, &len);
    forward(len);
  }
  return *this;
}

#ifndef __KERNEL__
StringAccum &
StringAccum::operator<<(double d)
{
  if (char *x = reserve(256)) {
    int len;
    sprintf(x, "%g%n", d, &len);
    forward(len);
  }
  return *this;
}
#endif

StringAccum &
StringAccum::operator<<(unsigned long long q)
{
  String qstr = cp_unparse_ulonglong(q, 10, false);
  return *this << qstr;
}
