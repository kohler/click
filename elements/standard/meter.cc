/*
 * packetmeter.{cc,hh} -- element sends packets out one of several outputs
 * depending on recent rate (packets/s)
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "meter.hh"
CLICK_DECLS

Meter::Meter()
{
}

void
Meter::push(int, Packet *p)
{
  _rate.update(1);		// packets, not bytes

  unsigned r = _rate.scaled_average();
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

CLICK_ENDDECLS
ELEMENT_REQUIRES(BandwidthMeter)
EXPORT_ELEMENT(Meter)
