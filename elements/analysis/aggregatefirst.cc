/*
 * aggregatefirst.{cc,hh} -- output the first packet per aggregate
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
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
#include "aggregatefirst.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

AggregateFirst::AggregateFirst()
    : _agg_notifier(0)
{
    memset(_kills, 0, sizeof(_kills));
    memset(_counts, 0, sizeof(_counts));
}

AggregateFirst::~AggregateFirst()
{
}

int
AggregateFirst::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e = 0;

    if (Args(conf, this, errh)
	.read("NOTIFIER", e)
	.complete() < 0)
	return -1;

    if (e && !(_agg_notifier = (AggregateNotifier *)e->cast("AggregateNotifier")))
	return errh->error("%s is not an AggregateNotifier", e->name().c_str());

    return 0;
}

int
AggregateFirst::initialize(ErrorHandler *)
{
    if (_agg_notifier)
	_agg_notifier->add_listener(this);
    return 0;
}

void
AggregateFirst::cleanup(CleanupStage)
{
    for (int i = 0; i < NPLANE; i++) {
	if (uint32_t **p = _kills[i]) {
	    for (int j = 0; j < NCOL; j++)
		delete[] p[j];
	    delete[] p;
	}
	delete[] _counts[i];
    }
}

uint32_t *
AggregateFirst::create_row(uint32_t agg)
{
    int planeno = (agg >> PLANE_SHIFT) & PLANE_MASK;
    if (!_kills[planeno]) {
	if (!(_kills[planeno] = new uint32_t *[NCOL]))
	    return 0;
	memset(_kills[planeno], 0, sizeof(uint32_t *) * NCOL);
	if (!_agg_notifier)
	    /* skip */;
	else if (!(_counts[planeno] = new uint32_t[NCOL + 1])) {
	    delete[] _kills[planeno];
	    _kills[planeno] = 0;
	    return 0;
	} else
	    memset(_counts[planeno], 0, sizeof(uint32_t) * (NCOL + 1));
    }
    uint32_t **plane = _kills[planeno];

    int colno = (agg >> COL_SHIFT) & COL_MASK;
    if (!plane[colno]) {
	if (!(plane[colno] = new uint32_t[NROW]))
	    return 0;
	memset(plane[colno], 0, sizeof(uint32_t) * NROW);
    }

    return plane[colno];
}

inline Packet *
AggregateFirst::smaction(Packet *p)
{
    uint32_t agg = AGGREGATE_ANNO(p);

    if (uint32_t *r = row(agg)) {
	r += (agg & ROW_MASK) >> 5;
	uint32_t mask = 1 << (agg & 0x1F);
	if (!(*r & mask)) {
	    *r |= mask;
	    return p;
	}
    }

    checked_output_push(1, p);
    return 0;
}

void
AggregateFirst::push(int, Packet *p)
{
    if ((p = smaction(p)))
	output(0).push(p);
}

Packet *
AggregateFirst::pull(int)
{
    Packet *p = input(0).pull();
    if (p)
	p = smaction(p);
    return p;
}

void
AggregateFirst::aggregate_notify(uint32_t agg, AggregateEvent event, const Packet *)
{
    int plane = (agg >> PLANE_SHIFT) & PLANE_MASK;
    int col = (agg >> COL_SHIFT) & COL_MASK;
    uint32_t *r = row(agg);
    if (!r)			// out of memory
	return;

    if (event == NEW_AGG) {
	if ((++_counts[plane][col]) == 1)
	    _counts[plane][NCOL]++;
    } else if (event == DELETE_AGG) {
	r[(agg & ROW_MASK) >> 5] &= ~(1 << (agg & 0x1F));
	if ((--_counts[plane][col]) == 0) {
	    // get rid of empty row
	    delete[] _kills[plane][col];
	    _kills[plane][col] = 0;
	    // get rid of empty column
	    if ((--_counts[plane][NCOL]) == 0) {
		delete[] _counts[plane];
		_counts[plane] = 0;
		delete[] _kills[plane];
		_kills[plane] = 0;
	    }
	}
    }
}

ELEMENT_REQUIRES(userlevel AggregateNotifier)
EXPORT_ELEMENT(AggregateFirst)
CLICK_ENDDECLS
