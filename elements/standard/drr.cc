// -*- c-basic-offset: 4 -*-
/*
 * drr.{cc,hh} -- deficit round-robin scheduler
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/error.hh>
#include <click/args.hh>
#include "drr.hh"
CLICK_DECLS

DRRSched::DRRSched()
    : _quantum(500), _pi(0),
      _notifier(Notifier::SEARCH_CONTINUE_WAKE), _next(0)
{
}

void *
DRRSched::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
	return &_notifier;
    else
	return Element::cast(n);
}

int
DRRSched::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _notifier.initialize(Notifier::EMPTY_NOTIFIER, router());
    if (Args(conf, this, errh).read_p("QUANTUM", _quantum).complete() < 0)
	return -1;
    if (_quantum <= 0)
	return errh->error("bad QUANTUM");
    return 0;
}

int
DRRSched::initialize(ErrorHandler *errh)
{
    if (!(_pi = new portinfo[ninputs()]))
	return errh->error("out of memory!");
    for (int i = 0; i < ninputs(); i++) {
	_pi[i].head = 0;
	_pi[i].deficit = 0;
	_pi[i].signal = Notifier::upstream_empty_signal(this, i, &_notifier);
    }
    _next = 0;
    return 0;
}

void
DRRSched::cleanup(CleanupStage)
{
    if (_pi) {
	for (int j = 0; j < ninputs(); j++)
	    if (_pi[j].head)
		_pi[j].head->kill();
	delete[] _pi;
    }
}

Packet *
DRRSched::pull(int)
{
    int n = ninputs();
    bool signals_on = false;

    // Look at each input once, starting at the *same*
    // one we left off on last time.
    for (int j = 0; j < n; j++) {
	portinfo &pi = _pi[_next];
	Packet *p;
	if ((p = pi.head)) {
	    pi.head = 0;
	    signals_on = true;
	} else if (pi.signal) {
	    p = input(_next).pull();
	    signals_on = true;
	} else
	    p = 0;

	if (p == 0)
	    pi.deficit = 0;
	else if (p->length() <= pi.deficit) {
	    pi.deficit -= p->length();
	    _notifier.set_active(true);
	    return p;
	} else
	    pi.head = p;

	_next++;
	if (_next >= n)
	    _next = 0;
	_pi[_next].deficit += _quantum;
    }

    _notifier.set_active(signals_on);
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DRRSched)
