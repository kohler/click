/*
 * bandwidthshaper.{cc,hh} -- element limits number of successful pulls per
 * second to a given rate (bytes/s)
 * Benjie Chen, Eddie Kohler
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
#include <click/config.h>
#include <click/package.hh>
#include "bandwidthshaper.hh"

BandwidthShaper::BandwidthShaper()
{
  // no MOD_INC_USE_COUNT; rely on Shaper
}

BandwidthShaper::~BandwidthShaper()
{
  // no MOD_DEC_USE_COUNT; rely on Shaper
}

BandwidthShaper *
BandwidthShaper::clone() const
{
  return new BandwidthShaper;
}

Packet *
BandwidthShaper::pull(int)
{
  Packet *p = 0;
  struct timeval now;
  click_gettimeofday(&now);
  if (_rate.need_update(now)) {
    if ((p = input(0).pull()))
      _rate.update_with(p->length());
  }
  return p;
}

EXPORT_ELEMENT(BandwidthShaper)
