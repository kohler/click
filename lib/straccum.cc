/*
 * straccum.{cc,hh} -- build up strings with operator<<
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/straccum.hh>
#include <click/string.hh>
#include <click/confparse.hh>
#include <click/glue.hh>

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

StringAccum &
StringAccum::operator<<(long i)
{
  if (char *x = reserve(256)) {
    int len;
    sprintf(x, "%ld%n", i, &len);
    forward(len);
  }
  return *this;
}

StringAccum &
StringAccum::operator<<(unsigned long u)
{
  if (char *x = reserve(256)) {
    int len;
    sprintf(x, "%lu%n", u, &len);
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
