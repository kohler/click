/*
 * bandwidthshaper.{cc,hh} -- element limits number of successful pulls per
 * second to a given rate (bytes/s)
 * Benjie Chen, Eddie Kohler
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
#include "bandwidthshaper.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

BandwidthShaper *
BandwidthShaper::clone() const
{
  return new BandwidthShaper;
}

Packet *
BandwidthShaper::pull(int)
{
  struct timeval now;
  Packet *p = 0;
  
  click_gettimeofday(&now);
  if (now.tv_sec > _tv_sec) {
    _tv_sec = now.tv_sec;
    if (_count > 0)
      _count -= _rate;
  }

  unsigned need = (now.tv_usec << UGAP_SHIFT) / _ugap;
  if ((int)need >= _count) {
    if ((p = input(0).pull()))
      _count += p->length();
  }
  
  return p;
}

EXPORT_ELEMENT(BandwidthShaper)
