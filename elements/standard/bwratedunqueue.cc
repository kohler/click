/*
 * ratedunqueue.{cc,hh} -- element pulls as many packets as possible from
 * its input, pushes them out its output
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
#include "bwratedunqueue.hh"

void
BandwidthRatedUnqueue::run_scheduled()
{
  struct timeval now;
  click_gettimeofday(&now);
  if (_rate.need_update(now)) {
    if (Packet *p = input(0).pull()) {
      _rate.update_with(p->length());
      output(0).push(p);
    }
  }
}

EXPORT_ELEMENT(BandwidthRatedUnqueue)
