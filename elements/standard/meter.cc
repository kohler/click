/*
 * packetmeter.{cc,hh} -- element sends packets out one of several outputs
 * depending on recent rate (packets/s)
 * Eddie Kohler
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
#include "meter.hh"

Meter::Meter()
{
  // no MOD_INC_USE_COUNT; rely on BandwidthMeter
}

Meter::~Meter()
{
  // no MOD_DEC_USE_COUNT; rely on BandwidthMeter
}

Meter *
Meter::clone() const
{
  return new Meter;
}

void
Meter::push(int, Packet *p)
{
  _rate.update(1);		// packets, not bytes

  unsigned r = _rate.average();
  if (_nmeters < 2) {
    int n = (r >= _meter1);
    output(n).push(p);
  } else {
    unsigned *meters = _meters;
    int nmeters = _nmeters;
    for (int i = 0; i < nmeters; i++)
      if (r < meters[i]) {
	output(i).push(p);
	return;
      }
    output(nmeters).push(p);
  }
}

EXPORT_ELEMENT(Meter)
