/*
 * packetmeter.{cc,hh} -- element sends packets out one of several outputs
 * depending on recent rate (packets/s)
 * Eddie Kohler
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
#include "meter.hh"

Meter::Meter()
{
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
