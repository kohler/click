/*
 * settimestamp.{cc,hh} -- set timestamp annotations
 * Douglas S. J. De Couto
 * based on setperfcount.{cc,hh}
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

#include <click/config.h>
#include <click/package.hh>
#include "settimestamp.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

SetTimestamp::SetTimestamp()
{
  // no MOD_INC_USE_COUNT; rely on PerfCountUser
  add_input();
  add_output();
}

SetTimestamp::~SetTimestamp()
{
  // no MOD_DEC_USE_COUNT; rely on PerfCountUser
}

SetTimestamp *
SetTimestamp::clone() const
{
  return new SetTimestamp();
}

void *
SetTimestamp::cast(const char *n)
{
  if (strcmp(n, "SetTimestamp") == 0)
    return (Element *)this;
  else
    return 0;
}

int
SetTimestamp::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _secs = -1;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpInteger, "seconds", _secs,
		  cpInteger, "microseconds", _usecs,
		  cpEnd) < 0)
    return -1;

  return 0;
}

inline void
SetTimestamp::smaction(Packet *p)
{
  struct timeval tv;
  if (_secs >= 0) {
    tv.tv_sec = _secs;
    tv.tv_usec = _usecs;
  } 
  else {
    int res = gettimeofday(&tv, 0);
    if (res != 0) {
      click_chatter("%s: gettimeofday failed", id().cc());
      return;
    }
  }
  p->set_timestamp_anno(tv);
}

void
SetTimestamp::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
SetTimestamp::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    smaction(p);
  return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SetTimestamp)
