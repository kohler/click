/*
 * shaper.{cc,hh} -- element limits number of successful pulls per
 * second to a given rate (packets/s)
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "shaper.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

Shaper::Shaper()
{
  add_input();
  add_output();
}

Shaper::~Shaper()
{
}

Shaper *
Shaper::clone() const
{
  return new Shaper;
}

int
Shaper::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned rate;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "max allowable rate", &rate,
		  0) < 0)
    return -1;

  unsigned one_sec = (1000000U << UGAP_SHIFT);
  if (rate > one_sec) {
    // must have _ugap > 0, so limit rate to 1e6<<UGAP_SHIFT
    errh->error("rate too large; lowered to %u", one_sec);
    rate = one_sec;
  }

  _ugap = one_sec / rate;
  _count = 0;
  _rate = rate;
  return 0;
}

int
Shaper::initialize(ErrorHandler *)
{
  struct timeval now;
  click_gettimeofday(&now);
  _tv_sec = now.tv_sec;
  _count = (now.tv_usec << UGAP_SHIFT) / _ugap;
  return 0;
}

Packet *
Shaper::pull(int)
{
  struct timeval now;
  Packet *p = 0;
  
  click_gettimeofday(&now);
  if (now.tv_sec > _tv_sec) {
    _tv_sec = now.tv_sec;
    if (_count > 0)
      _count -= _rate;
  }

  unsigned need = (now.tv_usec << UGAP_SHIFT) / _ugap;
  if ((int)need >= _count) {
    if ((p = input(0).pull()))
      _count++;
  }
  
  return p;
}

EXPORT_ELEMENT(Shaper)
