/*
 * timeutils.hh
 * File created by Douglas S. J. De Couto
 * 15 November 2001
 *
 * C++ utility routines for working Posix timespec structs (man
 * clock_gettime for more info).
 *
 * A large portion of this code is stolen from the file amisc.h in the
 * SFS asynchronous I/O library, and is covered by the following
 * copyright:
 *
 * Copyright (C) 1998 David Mazieres (dm@uun.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA 
 */

#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include <cmath>
#include <sys/time.h>
#include <cstdio>
#include <click/string.hh>
#include <click/glue.hh>	/* for htonl() and ntohl() */
CLICK_DECLS

inline timeval
hton(const timeval &tv) {
  struct timeval tv2;
  tv2.tv_sec = htonl(tv.tv_sec);
  tv2.tv_usec = htonl(tv.tv_usec);
  return tv2;
}

inline timeval
ntoh(const timeval &tv) {
  struct timeval tv2;
  tv2.tv_sec = ntohl(tv.tv_sec);
  tv2.tv_usec = ntohl(tv.tv_usec);
  return tv2;
}

inline String
timeval_to_str(const timeval &tv)
{
  char buf[80];
  snprintf(buf, sizeof(buf), "%lu.%06lu", tv.tv_sec, tv.tv_usec);
  return buf;
}


inline timeval
double_to_timeval(double t)
{
  timeval tv;
  tv.tv_sec = (time_t) floor(t);
  double frac = t - tv.tv_sec;
  tv.tv_usec = (long) (frac * 1e6);
  return tv;
}

inline double
timeval_to_double(const timeval &tv)
{
  double d = tv.tv_sec;
  double frac = tv.tv_usec;
  return d + frac * 1e-6;
}

CLICK_ENDDECLS
#endif 
