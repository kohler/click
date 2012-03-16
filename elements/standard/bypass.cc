/*
 * bypass.{cc,hh} -- bypass element
 * Eddie Kohler (idea with Cliff Frey)
 *
 * Copyright (c) 2012 Meraki, Inc.
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
#include "bypass.hh"
#include <click/router.hh>
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

Bypass::Bypass()
    : _active(false), _inline(false)
{
}

void *
Bypass::cast(const char *name)
{
    if (strcmp(name, "Bypass") == 0)
	return this;
    return Element::cast(name);
}

int
Bypass::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	.read_p("ACTIVE", _active)
	.read("INLINE", _inline)
	.complete() < 0)
	return -1;
    return 0;
}

int
Bypass::initialize(ErrorHandler *)
{
    fix();
    return 0;
}

void
Bypass::push(int port, Packet *p)
{
    output(_active && !port).push(p);
}

Packet *
Bypass::pull(int port)
{
    return input(_active && !port).pull();
}

bool
Bypass::Visitor::visit(Element *e, bool isoutput, int port,
		       Element *, int, int)
{
    if (!_applying) {
	_e = e;
	_port = port;
    } else
	// Just cheat.
	const_cast<Element::Port &>(e->port(isoutput, port)).assign(isoutput, _e, _port);
    return false;
}

void
Bypass::fix()
{
    if (!_inline) {
	bool direction = output_is_push(0);
	Visitor v(this);
	while (Bypass *b = static_cast<Bypass *>(v._e->cast("Bypass")))
	    router()->visit(b, direction,
			    b->_active ? b->nports(direction) - 1 : 0, &v);
	v._applying = true;
	router()->visit(this, !direction, 0, &v);
    }
}

int
Bypass::write_handler(const String &s, Element *e, void *, ErrorHandler *errh)
{
    Bypass *b = static_cast<Bypass *>(e);
    bool active;
    if (!BoolArg().parse(s, active))
	return errh->error("syntax error");
    if (active != b->_active) {
	b->_active = active;
	b->fix();
    }
    return 0;
}

void
Bypass::add_handlers()
{
    add_data_handlers("active", Handler::OP_READ, &_active);
    add_write_handler("active", write_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Bypass)

