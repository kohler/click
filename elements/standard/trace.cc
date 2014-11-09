/*
 * trace.{cc,hh} -- element that marks packets for debug tracing
 * Patrick Verkaik
 *
 * Copyright (c) 2014 Meraki, Inc.
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include "trace.hh"
CLICK_DECLS

bool Trace::_global_preempt = false;

Trace::Trace()
{
}

Trace::~Trace()
{
}

int
Trace::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _limit = 1;
    _count = 0;
    _no_clone = false;
    _preempt = false;
    _ignore_global_preempt = false;

    if (Args(conf, this, errh)
	.read_p("LIMIT", _limit)
	.read("NO_CLONE", _no_clone)
	.read("PREEMPT", _preempt)
	.read("IGNORE_GLOBAL_PREEMPT", _ignore_global_preempt)
	.complete() < 0)
	return -1;

    if (_no_clone && input_is_pull(0))
	return errh->error("NO_CLONE not valid in pull context");

    return 0;
}

// returns true if p was pushed out
bool
Trace::process(Packet *p)
{
    if ((_limit >= 0 && _count >= (unsigned) _limit) || !p->set_traced(_preempt || (_global_preempt && !_ignore_global_preempt)))
	return false;

    click_chatter("traced_packet: started (%p{element})\n", this);
    ++_count;
    if (noutputs() > 1) {
	if (!_no_clone) {
	    Packet *q = p->clone();
	    if (q)
		output(1).push(q);
	} else {
	    output(1).push(p);
	    return true;
	}
    }
    return false;
}

void
Trace::push(int, Packet *p)
{
    if (!process(p))
	output(0).push(p);
}

Packet *
Trace::pull(int)
{
    Packet *p = input(0).pull();
    if (!p)
	return 0;
    process(p);
    return p;
}

int
Trace::write_param(const String &, Element *e, void *thunk_p, ErrorHandler *)
{
    Trace *elt = (Trace *) e;
    intptr_t thunk = (intptr_t) thunk_p;

    switch (thunk) {
    case h_reset_count:
	elt->_count = 0;
	break;
    case h_poke:
	if (elt->_limit == 0)
	    elt->_limit = 1;
	elt->_count = 0;
	Packet::clear_traced();
	break;
    case h_clear_traced:
	Packet::clear_traced();
	break;
    default:
	assert(0);
    }
    return 0;
}

String
Trace::read_param(Element *, void *thunk_p)
{
    uintptr_t thunk = (uintptr_t) thunk_p;

    switch (thunk) {
    case h_have_tracing:
#if HAVE_PACKET_TRACING
	return String(true);
#else
	return String(false);
#endif
        break;
    default:
	assert(0);
    }
    return String::make_empty();
}

void
Trace::add_handlers()
{
    add_data_handlers("limit", Handler::OP_READ | Handler::OP_WRITE, &_limit);
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_data_handlers("preempt", Handler::OP_READ | Handler::OP_WRITE, &_preempt);
    add_data_handlers("ignore_global_preempt", Handler::OP_READ | Handler::OP_WRITE, &_ignore_global_preempt);
    add_data_handlers("global_preempt", Handler::OP_READ | Handler::OP_WRITE, &_global_preempt);

    add_write_handler("reset_count", write_param, h_reset_count);
    add_write_handler("poke", write_param, h_poke);
    add_write_handler("global_clear_traced", write_param, h_clear_traced);
    add_read_handler("global_have_tracing", read_param, h_have_tracing);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Trace)
