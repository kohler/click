/*
 * shaper.{cc,hh} -- element limits number of successful pulls per
 * second to a given rate (packets/s)
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "shaper.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

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
