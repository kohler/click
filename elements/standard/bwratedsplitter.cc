/*
 * bwratedsplitter.{cc,hh} -- split packets at a given bandwidth rate.
 * Benjie Chen, Eddie Kohler
 * 
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

#include <click/config.h>
#include <click/package.hh>
#include "bwratedsplitter.hh"

BandwidthRatedSplitter::BandwidthRatedSplitter()
{
  // no MOD_INC_USE_COUNT; rely on RatedSplitter
}

BandwidthRatedSplitter::~BandwidthRatedSplitter()
{
  // no MOD_DEC_USE_COUNT; rely on RatedSplitter
}

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
