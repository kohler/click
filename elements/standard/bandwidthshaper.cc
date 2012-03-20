// -*- c-basic-offset: 4 -*-
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
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "bandwidthshaper.hh"
CLICK_DECLS

BandwidthShaper::BandwidthShaper()
{
}

Packet *
BandwidthShaper::pull(int)
{
    Packet *p = 0;
    if (_rate.need_update(Timestamp::now())) {
	if ((p = input(0).pull()))
	    _rate.update_with(p->length());
    }
    return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Shaper)
EXPORT_ELEMENT(BandwidthShaper)
