/*
 * packetshaper2.{cc,hh} -- element limits number of successful pulls per
 * second to a given rate (packets/s). unlike packetshaper, does not use EWMA.
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
#include "packetshaper2.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

PacketShaper2::PacketShaper2()
{
  add_input();
  add_output();
}

PacketShaper2::~PacketShaper2()
{
}

PacketShaper2 *
PacketShaper2::clone() const
{
  return new PacketShaper2;
}

int
PacketShaper2::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int rate;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "max allowable rate", &rate,
		  0) < 0)
    return -1;

  if (rate > 1000000) 
    return errh->error("rate must be less than 1000000");
 
  _interval = 1000000/rate;
  _last = 0;
  return 0;
}

Packet *
PacketShaper2::pull(int)
{
  struct timeval t;
  click_gettimeofday(&t);
  unsigned now = t.tv_sec * 1000000 + t.tv_usec;
  
  bool need = (_last == 0) || (now - _last) > _interval;

  if (need) {
    if (_last == 0) 
      _last = now;
    else
      _last += _interval;
    Packet *p = input(0).pull();
    return p;
  }
  return 0;
}

EXPORT_ELEMENT(PacketShaper2)
