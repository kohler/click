/*
 * straccum.{cc,hh} -- build up strings with operator<<
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include <click/straccum.hh>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/confparse.hh>

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
StringAccum::c_str()
{
  append('\0');
  pop_back();
  return reinterpret_cast<char *>(_s);
}

String
StringAccum::take_string()
{
  int len = length();
  return String::claim_string(take(), len);
}

StringAccum &
operator<<(StringAccum &sa, const char *s)
{
  sa.append(s, strlen(s));
  return sa;
}

StringAccum &
operator<<(StringAccum &sa, long i)
{
  if (char *x = sa.reserve(24)) {
    int len = sprintf(x, "%ld", i);
    sa.forward(len);
  }
  return sa;
}

StringAccum &
operator<<(StringAccum &sa, unsigned long u)
{
  if (char *x = sa.reserve(24)) {
    int len = sprintf(x, "%lu", u);
    sa.forward(len);
  }
  return sa;
}

#ifdef HAVE_INT64_TYPES
StringAccum &
operator<<(StringAccum &sa, int64_t q)
{
  String qstr = cp_unparse_integer64(q, 10, false);
  return sa << qstr;
}

StringAccum &
operator<<(StringAccum &sa, uint64_t q)
{
  String qstr = cp_unparse_unsigned64(q, 10, false);
  return sa << qstr;
}
#endif

#ifndef __KERNEL__
StringAccum &
operator<<(StringAccum &sa, double d)
{
  if (char *x = sa.reserve(256)) {
    int len = sprintf(x, "%g", d);
    sa.forward(len);
  }
  return sa;
}
#endif

StringAccum &
operator<<(StringAccum &sa, const struct timeval &tv)
{
  if (char *x = sa.reserve(30)) {
    int len = sprintf(x, "%ld.%06ld", (long)tv.tv_sec, (long)tv.tv_usec);
    sa.forward(len);
  }
  return sa;
}
