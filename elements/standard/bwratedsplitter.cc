/*
 * bwratedsplitter.{cc,hh} -- split packets at a given bandwidth rate.
 * Benjie Chen, Eddie Kohler
 * 
 * Copyright (c) 2000 Mazu Networks, Inc.
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
