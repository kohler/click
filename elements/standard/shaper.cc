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
  _rate.set_rate(rate, errh);
  return 0;
}

Packet *
Shaper::pull(int)
{
  Packet *p = 0;
  struct timeval now;
  click_gettimeofday(&now);
  if (_rate.need_update(now)) {
    if ((p = input(0).pull()))
      _rate.update();
  }
  return p;
}

EXPORT_ELEMENT(Shaper)
