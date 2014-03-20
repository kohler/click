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
    output(_active && !port && noutputs() > 1).push(p);
}

Packet *
Bypass::pull(int port)
{
    return input(_active && !port && ninputs() > 1).pull();
}

Bypass::Locator::Locator(bool active)
    : _e(0), _port(active) {
}

bool
Bypass::Locator::visit(Element* e, bool isoutput, int port,
                       Element*, int from_port, int)
{
    if (from_port != _port)
        return false;
    if (Bypass* b = static_cast<Bypass*>(e->cast("Bypass")))
        if (!b->_inline) {
            _port = b->_active && port == 0 && b->nports(!isoutput) > 1;
            return true;
        }
    _e = e;
    _port = port;
    return false;
}

Bypass::Assigner::Assigner(Element* e, int port)
    : _e(e), _port(port) {
}

bool
Bypass::Assigner::visit(Element* e, bool isoutput, int port,
                        Element* from_e, int from_port, int)
{
    if (_interesting.empty()) {
        _interesting.push_back(from_e->eindex());
        _interesting.push_back(3);
    }
    for (int i = 0; i != _interesting.size(); i += 2)
        if (_interesting[i] == from_e->eindex()
            && (_interesting[i+1] & (1 << from_port)))
            goto found;
    return false;
 found:
    if (Bypass* b = static_cast<Bypass*>(e->cast("Bypass")))
        if (!b->_inline) {
            _interesting.push_back(b->eindex());
            _interesting.push_back((b->_active == port || b->nports(isoutput) == 1 ? 1 : 0)
                                   | (port == 0 ? 2 : 0));
            return true;
        }
    // Just cheat.
    //click_chatter("Bypass: Assigning %p{element}:%d to %p{element}:%d\n", e, port, _e, _port);
    const_cast<Element::Port&>(e->port(isoutput, port)).assign(isoutput, _e, _port);
    return false;
}

void
Bypass::fix()
{
    if (!_inline) {
	bool direction = output_is_push(0);
        Locator loc(_active);
        router()->visit(this, direction, _active, &loc);
        if (loc._e) {
            Assigner ass(loc._e, loc._port);
            router()->visit(this, !direction, _active ? 0 : -1, &ass);
        }
        if (_active && nports(!direction) > 1) {
            Locator loc(0);
            router()->visit(this, direction, 0, &loc);
            if (loc._e) {
                Assigner ass(loc._e, loc._port);
                router()->visit(this, !direction, 1, &ass);
            }
        }
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

