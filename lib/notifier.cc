// -*- c-basic-offset: 4; related-file-name: "../include/click/notifier.hh" -*-
/*
 * notifier.{cc,hh} -- activity notification
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
#include <click/notifier.hh>
#include <click/router.hh>
#include <click/element.hh>
#include <click/elemfilter.hh>

#define NUM_SIGNALS 4096
static uint32_t signals[NUM_SIGNALS / 32];

uint32_t NotifierSignal::true_value = 0xFFFFFFFFU;

Notifier::Notifier()
    : _listener1(0), _listeners(0)
{
}

int
Notifier::initialize(Router *r)
{
    void *&val = r->force_attachment("NotifierSignal count");
    uint32_t nsignals = (uint32_t) val;
    if (nsignals < NUM_SIGNALS) {
	_signal = NotifierSignal(&signals[nsignals / 32], 1 << (nsignals % 32));
	val = (void *)(nsignals + 1);
	return 0;
    } else
	return -1;
}

int
Notifier::add_listener(Task *new_l)
{
    if (!_listener1 && !_listeners) {
	_listener1 = new_l;
	return 0;
    } else if (new_l == 0)
	return 0;

    int n = (_listener1 ? 1 : 0);
    if (!_listener1)
	for (Task **t = _listeners; *t; t++)
	    n++;
    Task **old_list = (_listener1 ? &_listener1 : _listeners);
    
    Task **new_list = new Task *[n + 2];
    if (!new_list) {
	click_chatter("out of memory in Notifier::add_listener!");
	return -1;
    }
    memcpy(new_list, old_list, sizeof(Task *) * 1);
    new_list[n] = new_l;
    new_list[n + 1] = _listener1 = 0;
    delete[] _listeners;
    _listeners = new_list;
    return 0;
}

void
Notifier::remove_listener(Task *bad_l)
{
    if (!bad_l)
	/* nada */;
    else if (_listener1 == bad_l)
	_listener1 = 0;
    else if (_listeners) {
	int n = 0, which = -1;
	for (Task **l = _listeners; *l; l++) {
	    if (*l == bad_l)
		which = n;
	    n++;
	}
	if (which >= 0) {
	    _listeners[which] = _listeners[n - 1];
	    _listeners[n - 1] = 0;
	}
    }
}

NotifierSignal
Notifier::upstream_pull_signal(Element *e, int port, Task *t)
{
    CastElementFilter notifier_filter("Notifier");
    OutputProcessingElementFilter push_filter(true);
    DisjunctionElementFilter filter;
    filter.add(&notifier_filter);
    filter.add(&push_filter);
    
    Vector<Element *> v;
    int ok = e->router()->upstream_elements(e, port, &filter, v);

    // All bets are off if filter ran into a push output. That means there was
    // a regular Queue in the way (for example).
    if (push_filter.match_count())
	return NotifierSignal();
    
    notifier_filter.filter(v);

    NotifierSignal signal;
    if (ok >= 0 && v.size()) {
	for (int i = 0; ok >= 0 && i < v.size(); i++) {
	    Notifier *n = (Notifier *) (v[i]->cast("Notifier"));
	    n->add_listener(t);
	    if (i == 0)
		signal = n->activity_signal();
	    else
		signal += n->activity_signal();
	}
    }

    return signal;
}
