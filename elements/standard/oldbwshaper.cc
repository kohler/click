/*
 * bandwidthshaper.{cc,hh} -- element limits number of successful pulls
 * per second to a given rate (bytes/s)
 * Eddie Kohler
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
#include "bandwidthshaper.hh"
#include "confparse.hh"
#include "error.hh"

BandwidthShaper::BandwidthShaper()
{
  add_input();
  add_output();
}

BandwidthShaper::~BandwidthShaper()
{
}

BandwidthShaper *
BandwidthShaper::clone() const
{
  return new BandwidthShaper;
}

int
BandwidthShaper::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned rate;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "max allowable rate", &rate,
		  0) < 0)
    return -1;
  
  unsigned max_value = 0xFFFFFFFF >> _rate.scale;
  if (rate > max_value)
    return errh->error("rate too large (max %u)", max_value);
  
  _meter1 = (rate << _rate.scale) / _rate.freq();
  return 0;
}

int
BandwidthShaper::initialize(ErrorHandler *)
{
  _rate.initialize();

  return 0;
}

Packet *
BandwidthShaper::pull(int)
{
  _rate.update_time();
  
  unsigned r = _rate.average();
  if (r >= _meter1)
    return 0;
  else {
    Packet *p = input(0).pull();
    if (p) _rate.update_now(p->length());
    return p;
  }
}

static String
read_rate_handler(Element *f, void *)
{
  BandwidthShaper *c = (BandwidthShaper *)f;
  return cp_unparse_real(c->rate()*c->rate_freq(), c->rate_scale()) + "\n";
}

void
BandwidthShaper::add_handlers()
{
  add_read_handler("rate", read_rate_handler, 0);
}

ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(BandwidthShaper)
