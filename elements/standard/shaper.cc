/*
 * shaper.{cc,hh} -- element limits number of successful pulls
 * per second to a given rate (packets/s)
 * Eddie Kohler
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

Shaper::Shaper()
{
}

Shaper *
Shaper::clone() const
{
  return new Shaper;
}

Packet *
Shaper::pull(int)
{
  _rate.update_time();
  
  unsigned r = _rate.average();
  if (r >= _meter1)
    return 0;
  else {
    Packet *p = input(0).pull();
    if (p) {
      _rate.update_now(1);	// packets, not bytes
    }
    return p;
  }
}

EXPORT_ELEMENT(Shaper)
