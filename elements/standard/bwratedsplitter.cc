/*
 * bwratedsplitter.{cc,hh} -- split packets at a given bandwidth rate.
 * Benjie Chen, Eddie Kohler
 * 
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
#include "bwratedsplitter.hh"

void
BandwidthRatedSplitter::push(int, Packet *p)
{
  struct timeval now;
  click_gettimeofday(&now);
  if (_rate.need_update(now)) {
    _rate.update_with(p->length());
    output(0).push(p);
  } else
    output(1).push(p);
}

EXPORT_ELEMENT(BandwidthRatedSplitter)
