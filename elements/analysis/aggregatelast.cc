/*
 * aggregatelast.{cc,hh} -- output the last packet per aggregate
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
#include "aggregatelast.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>
CLICK_DECLS

AggregateLast::AggregateLast()
    : _agg_notifier(0), _clear_task(this), _needs_clear(0)
{
    memset(_packets, 0, sizeof(_packets));
    memset(_counts, 0, sizeof(_counts));
}

AggregateLast::~AggregateLast()
{
}

int
AggregateLast::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e = 0;
    _stop_after_clear = false;

    if (Args(conf, this, errh)
	.read("NOTIFIER", e)
	.read("STOP_AFTER_CLEAR", _stop_after_clear)
	.complete() < 0)
	return -1;

    if (e && !(_agg_notifier = (AggregateNotifier *)e->cast("AggregateNotifier")))
	return errh->error("%s is not an AggregateNotifier", e->name().c_str());

    return 0;
}

int
AggregateLast::initialize(ErrorHandler *)
{
    if (_agg_notifier)
	_agg_notifier->add_listener(this);
    _clear_task.initialize(this, false);
    return 0;
}

void
AggregateLast::cleanup(CleanupStage)
{
    for (int i = 0; i < NPLANE; i++) {
	if (Packet ***p = _packets[i]) {
	    for (int j = 0; j < NCOL; j++)
		if (Packet **q = p[j]) {
		    for (int k = 0; k < NROW; k++)
			if (q[k])
			    q[k]->kill();
		    delete[] q;
		}
	    delete[] p;
	}
	delete[] _counts[i];
    }
}

Packet **
AggregateLast::create_row(uint32_t agg)
{
    int planeno = (agg >> PLANE_SHIFT) & PLANE_MASK;
    if (!_packets[planeno]) {
	if (!(_packets[planeno] = new Packet **[NCOL]))
	    return 0;
	memset(_packets[planeno], 0, sizeof(Packet **) * NCOL);
	if (!_agg_notifier)
	    /* skip */;
	else if (!(_counts[planeno] = new uint32_t[NCOL + 1])) {
	    delete[] _packets[planeno];
	    _packets[planeno] = 0;
	    return 0;
	} else
	    memset(_counts[planeno], 0, sizeof(uint32_t) * (NCOL + 1));
    }
    Packet ***plane = _packets[planeno];

    int colno = (agg >> COL_SHIFT) & COL_MASK;
    if (!plane[colno]) {
	if (!(plane[colno] = new Packet *[NROW]))
	    return 0;
	memset(plane[colno], 0, sizeof(Packet *) * NROW);
    }

    return plane[colno];
}

void
AggregateLast::push(int, Packet *p)
{
    // clean if we should clean
    if (_clear_task.scheduled()) {
	_clear_task.unschedule();
	run_task(0);
    }

    uint32_t agg = AGGREGATE_ANNO(p);

    if (Packet **r = row(agg)) {
	static_assert(ROW_SHIFT == 0, "ROW_SHIFT failure.");
	r += (agg & ROW_MASK);
	if (*r) {
	    SET_EXTRA_PACKETS_ANNO(p, EXTRA_PACKETS_ANNO(p) + 1 + EXTRA_PACKETS_ANNO(*r));
	    SET_EXTRA_LENGTH_ANNO(p, EXTRA_LENGTH_ANNO(p) + (*r)->length() + EXTRA_LENGTH_ANNO(*r));
	    SET_FIRST_TIMESTAMP_ANNO(p, FIRST_TIMESTAMP_ANNO(*r));
	    checked_output_push(1, *r);
	} else
	    SET_FIRST_TIMESTAMP_ANNO(p, p->timestamp_anno());
	*r = p;
    }
}

void
AggregateLast::aggregate_notify(uint32_t agg, AggregateEvent event, const Packet *)
{
    int plane = (agg >> PLANE_SHIFT) & PLANE_MASK;
    int col = (agg >> COL_SHIFT) & COL_MASK;
    Packet **r = row(agg);
    if (!r)			// out of memory
	return;
    r += (agg & ROW_MASK);

    if (event == NEW_AGG) {
	if ((++_counts[plane][col]) == 1)
	    _counts[plane][NCOL]++;
    } else if (event == DELETE_AGG && *r) {
	// XXX should we push in a notify function? Well why not.
	output(0).push(*r);
	*r = 0;
	if ((--_counts[plane][col]) == 0) {
	    // get rid of empty row
	    delete[] _packets[plane][col];
	    _packets[plane][col] = 0;
	    // get rid of empty column
	    if ((--_counts[plane][NCOL]) == 0) {
		delete[] _counts[plane];
		_counts[plane] = 0;
		delete[] _packets[plane];
		_packets[plane] = 0;
	    }
	}
    }
}

bool
AggregateLast::run_task(Task *)
{
    if (!_needs_clear)
	return false;
    _needs_clear = 0;

    // may take a long time!
    for (int i = 0; i < NPLANE; i++)
	if (Packet ***p = _packets[i]) {
	    for (int j = 0; j < NCOL; j++)
		if (Packet **q = p[j]) {
		    for (int k = 0; k < NROW; k++)
			if (Packet *r = q[k])
			    output(0).push(r);
		    delete[] q;
		}
	    delete[] p;
	    if (_agg_notifier)
		delete[] _counts[i];
	}

    memset(_packets, 0, sizeof(_packets));
    memset(_counts, 0, sizeof(_counts));

    if (_stop_after_clear)
	router()->please_stop_driver();
    return true;
}

enum { H_CLEAR };

int
AggregateLast::write_handler(const String &, Element *e, void *thunk, ErrorHandler *)
{
    AggregateLast *al = reinterpret_cast<AggregateLast *>(e);
    switch (reinterpret_cast<intptr_t>(thunk)) {
      case H_CLEAR:
	al->_needs_clear = 1;
	al->_clear_task.reschedule();
	break;
    }
    return 0;
}

void
AggregateLast::add_handlers()
{
    add_write_handler("clear", write_handler, H_CLEAR);
}

ELEMENT_REQUIRES(userlevel AggregateNotifier)
EXPORT_ELEMENT(AggregateLast)
CLICK_ENDDECLS
