// -*- c-basic-offset: 4 -*-
/*
 * bwratedsplitter.{cc,hh} -- split packets at a given bandwidth rate.
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2010 Meraki, Inc.
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
#include "ratedunqueue.hh"
CLICK_DECLS

BandwidthRatedSplitter::BandwidthRatedSplitter()
{
}

void
BandwidthRatedSplitter::push(int, Packet *p)
{
    _tb.refill();
    if (_tb.contains(RatedUnqueue::tb_bandwidth_thresh)) {
	_tb.remove(p->length());
	output(0).push(p);
    } else
	checked_output_push(1, p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RatedSplitter)
EXPORT_ELEMENT(BandwidthRatedSplitter)
