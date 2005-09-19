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
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

StrideSched::StrideSched()
{
    _list = new Client;
    _list->make_head();
}

StrideSched::~StrideSched()
{
    delete _list;
}

int
StrideSched::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (processing() == PULL) {
	if (conf.size() != ninputs())
	    return errh->error("need %d arguments, one per input port", ninputs());
    } else {
	if (conf.size() != noutputs())
	    return errh->error("need %d arguments, one per output port", noutputs());
    }

    int before = errh->nerrors();
    for (int i = 0; i < conf.size(); i++) {
	int v;
	if (!cp_integer(conf[i], &v))
	    errh->error("argument %d should be number of tickets (integer)", i);
	else if (v < 0)
	    errh->error("argument %d (number of tickets) must be >= 0", i);
	else if (v == 0)
	    /* do not ever schedule it */;
	else {
	    if (v > MAX_TICKETS) {
		errh->warning("input %d's tickets reduced to %d", i, MAX_TICKETS);
		v = MAX_TICKETS;
	    }
	    _list->insert(new Client(i, v));
	}
    }
    return (errh->nerrors() == before ? 0 : -1);
}

int
StrideSched::initialize(ErrorHandler *)
{
    for (Client *c = _list->_next; c != _list; c = c->_next)
	c->_signal = Notifier::upstream_empty_signal(this, c->_port, 0);
    return 0;
}

void
StrideSched::cleanup(CleanupStage)
{
    while (_list->_next != _list) {
	Client *c = _list->_next;
	_list->_next->remove();
	delete c;
    }
}

Packet *
StrideSched::pull(int)
{
    // go over list until we find a packet, striding as we go
    Client *stridden = _list->_next;
    Client *c = stridden;
    Packet *p = 0;
    while (c != _list && !p) {
	if (c->_signal)
	    p = input(c->_port).pull();
	c->stride();
	c = c->_next;
    }

    // remove stridden portion from list
    _list->_next = c;
    c->_prev = _list;

    // reinsert stridden portion into list
    while (stridden != c) {
	Client *next = stridden->_next;
	_list->insert(stridden); // 'insert' is OK even when 'stridden's next
				// and prev pointers are garbage
	stridden = next;
    }

    return p;
}

int
StrideSched::tickets(int port) const
{
    for (Client *c = _list->_next; c != _list; c = c->_next)
	if (c->_port == port)
	    return c->_tickets;
    if (port >= 0 && port < ninputs())
	return 0;
    return -1;
}

int
StrideSched::set_tickets(int port, int tickets, ErrorHandler *errh)
{
    if (port < 0 || port >= ninputs())
	return errh->error("port %d out of range", port);
    else if (tickets < 0)
	return errh->error("number of tickets must be >= 0");
    else if (tickets > MAX_TICKETS) {
	errh->warning("port %d's tickets reduced to %d", port, MAX_TICKETS);
	tickets = MAX_TICKETS;
    }

    if (tickets == 0) {
	// delete Client
	for (Client *c = _list; c != _list; c = c->_next)
	    if (c->_port == port) {
		c->remove();
		delete c;
		return 0;
	    }
	return 0;
    }

    for (Client *c = _list->_next; c != _list; c = c->_next)
	if (c->_port == port) {
	    c->set_tickets(tickets);
	    return 0;
	}
    Client *c = new Client(port, tickets);
    c->_pass = _list->_next->_pass;
    _list->insert(c);
    return 0;
}

static String
read_tickets_handler(Element *e, void *thunk)
{
    StrideSched *ss = (StrideSched *)e;
    int port = (intptr_t)thunk;
    return String(ss->tickets(port)) + "\n";
}

static int
write_tickets_handler(const String &in_s, Element *e, void *thunk, ErrorHandler *errh)
{
    StrideSched *ss = (StrideSched *)e;
    int port = (intptr_t)thunk;
    String s = cp_uncomment(in_s);
    int tickets;
    if (!cp_integer(s, &tickets))
	return errh->error("tickets value must be integer");
    else
	return ss->set_tickets(port, tickets, errh);
}

void
StrideSched::add_handlers()
{
    for (int i = 0; i < ninputs(); i++) {
	String s = "tickets" + String(i);
	add_read_handler(s, read_tickets_handler, (void *)i);
	add_write_handler(s, write_tickets_handler, (void *)i);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StrideSched)
