// -*- c-basic-offset: 4 -*-
/*
 * stridesched.{cc,hh} -- stride scheduler
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "stridesched.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
CLICK_DECLS

StrideSched::StrideSched()
    : _all(0), _list(0)
{
}

int
StrideSched::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() != nclients())
	return errh->error("need %d arguments, one per %s port", nclients(),
			   (processing() == PULL ? "input" : "output"));

    bool first = !_all;
    if (first && !(_all = new Client[nclients()]))
	return errh->error("out of memory");

    for (int i = 0; i < conf.size(); i++) {
	int v;
	if (!IntArg().parse(conf[i], v))
	    errh->error("argument %d should be number of tickets (integer)", i);
	else if (v < 0)
	    errh->error("argument %d (number of tickets) must be >= 0", i);
	else {
	    if (v > MAX_TICKETS) {
		errh->warning("input %d%,s tickets reduced to %d", i, MAX_TICKETS);
		v = MAX_TICKETS;
	    }
	    _all[i].set_tickets(v);
	    if (first)
		_all[i].stride();
	}
    }

    // insert into reverse order so they're run in forward order on ties
    _list = 0;
    for (int i = nclients() - 1; i >= 0; i--)
	if (_all[i]._tickets)
	    _all[i].insert(&_list);

    return errh->nerrors() ? -1 : 0;
}

int
StrideSched::initialize(ErrorHandler *)
{
    if (input_is_pull(0))
	for (int i = 0; i < nclients(); ++i)
	    _all[i]._signal = Notifier::upstream_empty_signal(this, i);
    return 0;
}

void
StrideSched::cleanup(CleanupStage)
{
    delete[] _all;
}

Packet *
StrideSched::pull(int)
{
    // go over list until we find a packet, striding as we go
    Client *stridden = _list, *c;
    Packet *p = 0;
    for (c = _list; c && !p; c = c->_next) {
	if (c->_signal)
	    p = input(c - _all).pull();
	c->stride();
    }

    // remove stridden portion from list
    if ((_list = c))
	c->_pprev = &_list;

    // reinsert stridden portion into list
    while (stridden != c) {
	Client *next = stridden->_next;
	stridden->insert(&_list);
	stridden = next;
    }

    return p;
}

int
StrideSched::tickets(int port) const
{
    if ((unsigned) port < (unsigned) nclients())
	return _all[port]._tickets;
    else
	return -1;
}

int
StrideSched::set_tickets(int port, int tickets, ErrorHandler *errh)
{
    if ((unsigned) port >= (unsigned) nclients())
	return errh->error("port %d out of range", port);
    else if (tickets < 0)
	return errh->error("number of tickets must be >= 0");
    else if (tickets > MAX_TICKETS) {
	errh->warning("port %d%,s tickets reduced to %d", port, MAX_TICKETS);
	tickets = MAX_TICKETS;
    }

    int old_tickets = _all[port]._tickets;
    _all[port].set_tickets(tickets);

    if (tickets == 0 && old_tickets != 0)
	_all[port].remove();
    else if (tickets != 0 && old_tickets == 0) {
	_all[port]._pass = (_list ? _list->_pass + _all[port]._stride : 0);
	_all[port].insert(&_list);
    }
    return 0;
}

static String
read_tickets_handler(Element *e, void *thunk)
{
    StrideSched *ss = (StrideSched *)e;
    int port = (intptr_t)thunk;
    return String(ss->tickets(port));
}

static int
write_tickets_handler(const String &s, Element *e, void *thunk, ErrorHandler *errh)
{
    StrideSched *ss = (StrideSched *)e;
    int port = (intptr_t)thunk;
    int tickets;
    if (!IntArg().parse(s, tickets))
	return errh->error("tickets value must be integer");
    else
	return ss->set_tickets(port, tickets, errh);
}

String
StrideSched::read_handler(Element *e, void *)
{
    StrideSched *ss = static_cast<StrideSched *>(e);
    StringAccum sa;
    for (int i = 0; i < ss->nclients(); i++)
	sa << (i ? ", " : "") << ss->_all[i]._tickets;
    return sa.take_string();
}

void
StrideSched::add_handlers()
{
    // beware: StrideSwitch inherits from StrideSched
    for (intptr_t i = 0; i < nclients(); i++) {
	String s = "tickets" + String(i);
	add_read_handler(s, read_tickets_handler, i);
	add_write_handler(s, write_tickets_handler, i);
    }
    add_read_handler("config", read_handler);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StrideSched)
