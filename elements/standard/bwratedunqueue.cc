/*
 * ratedunqueue.{cc,hh} -- element pulls as many packets as possible from
 * its input, pushes them out its output
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "bwratedunqueue.hh"

BandwidthRatedUnqueue::BandwidthRatedUnqueue()
{
  // no MOD_INC_USE_COUNT; rely on RatedUnqueue
}

BandwidthRatedUnqueue::~BandwidthRatedUnqueue()
{
  // no MOD_DEC_USE_COUNT; rely on RatedUnqueue
}

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
  _task.fast_reschedule();
}

EXPORT_ELEMENT(BandwidthRatedUnqueue)
ELEMENT_MT_SAFE(BandwidthRatedUnqueue)
