/*
 * timeutils.hh
 * File created by Douglas S. J. De Couto
 * 15 November 2001
 */

#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#ifdef CLICK_USERLEVEL
#include <math.h>
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

CLICK_ENDDECLS
#endif
