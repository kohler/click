/*
 * packetshaper.{cc,hh} -- element limits number of successful pulls
 * per second to a given rate (packets/s)
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
#include "router.hh"
#include "packetshaper.hh"

PacketShaper::PacketShaper()
{
}

PacketShaper *
PacketShaper::clone() const
{
  return new PacketShaper;
}

Packet *
PacketShaper::pull(int)
{
  _rate.update_time();
  
  int r = _rate.average();
  if (r >= _meter1) {
#if 0
    if (_puller1)
      _puller1->join_scheduler();
    else {
      int n = _pullers.size();
      for (int i = 0; i < n; i++)
        _pullers[i]->join_scheduler();
    }
#endif
    return 0;
  } else {
    Packet *p = input(0).pull();
    if (p) _rate.update_now(1);	// packets, not bytes
    return p;
  }
}

EXPORT_ELEMENT(PacketShaper)
