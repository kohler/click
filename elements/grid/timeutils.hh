/*
 * timeutils.hh
 * File created by Douglas S. J. De Couto
 * 15 November 2001
 */

#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#ifdef CLICK_USERLEVEL
#include <cmath>
#endif
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
  snprintf(buf, sizeof(buf), "%ld.%06ld", (long) tv.tv_sec, (long) tv.tv_usec);
  return buf;
}

#ifdef CLICK_USERLEVEL
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
#endif

CLICK_ENDDECLS
#endif 
