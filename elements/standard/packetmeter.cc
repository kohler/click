/*
 * packetmeter.{cc,hh} -- element sends packets out one of several outputs
 * depending on recent rate (packets/s)
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "packetmeter.hh"

PacketMeter::PacketMeter()
{
}

PacketMeter *
PacketMeter::clone() const
{
  return new PacketMeter;
}

void
PacketMeter::push(int, Packet *p)
{
  _rate.update(1);		// packets, not bytes

  int r = _rate.average();
  if (_nmeters < 2) {
    int n = (r >= _meter1);
    output(n).push(p);
  } else {
    int *meters = _meters;
    int nmeters = _nmeters;
    for (int i = 0; i < nmeters; i++)
      if (r < meters[i]) {
	output(i).push(p);
	return;
      }
    output(nmeters).push(p);
  }
}

EXPORT_ELEMENT(PacketMeter)
