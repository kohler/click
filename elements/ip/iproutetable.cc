// -*- c-basic-offset: 4 -*-
/*
 * iproutetable.{cc,hh} -- looks up next-hop address in route table
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, subject to the conditions listed in the Click LICENSE
 * file. These conditions include: you must preserve this copyright
 * notice, and you cannot mention the copyright holders in advertising
 * related to the Software without their permission.  The Software is
 * provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This notice is a
 * summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "iproutetable.hh"
CLICK_DECLS

void *
IPRouteTable::cast(const char *name)
{
    if (strcmp(name, "IPRouteTable") == 0)
	return (void *)this;
    else
	return Element::cast(name);
}

int
IPRouteTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int before = errh->nerrors();
    for (int i = 0; i < conf.size(); i++) {
	Vector<String> words;
	cp_spacevec(conf[i], words);

	IPAddress dst, mask, gw;
	int32_t port;
	bool ok = false;
	if ((words.size() == 2 || words.size() == 3)
	    && cp_ip_prefix(words[0], &dst, &mask, true, this)
	    && cp_integer(words.back(), &port)) {
	    if (words.size() == 3)
		ok = cp_ip_address(words[1], &gw, this);
	    else
		ok = true;
	}

	if (ok && port >= 0)
	    (void) add_route(dst, mask, gw, port, errh);
	else
	    errh->error("argument %d should be `DADDR/MASK [GATEWAY] OUTPUT'", i+1);
    }

    return (errh->nerrors() != before ? -1 : 0);
}

int
IPRouteTable::add_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *errh)
{
    // by default, cannot add routes
    return errh->error("cannot add routes to this routing table");
}

int
IPRouteTable::remove_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *errh)
{
    // by default, cannot remove routes
    return errh->error("cannot delete routes from this routing table");
}

int
IPRouteTable::lookup_route(IPAddress, IPAddress &) const
{
    return -1;			// by default, route lookups fail
}

String
IPRouteTable::dump_routes() const
{
    return String();
}


void
IPRouteTable::push(int, Packet *p)
{
    IPAddress gw;
    int port = lookup_route(p->dst_ip_anno(), gw);

    if (port >= 0) {
	assert(port < noutputs());
	if (gw)
	    p->set_dst_ip_anno(gw);
	output(port).push(p);
    } else {
	click_chatter("IPRouteTable: no route for %s", p->dst_ip_anno().s().cc());
	p->kill();
    }
}


int
IPRouteTable::add_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    IPRouteTable *r = static_cast<IPRouteTable *>(e);

    Vector<String> words;
    cp_spacevec(conf, words);

    IPAddress dst, mask, gw;
    int port, ok;

    if (words.size() == 2)
	ok = cp_va_parse(words, r, errh,
			 cpIPAddressOrPrefix, "routing prefix", &dst, &mask,
			 cpInteger, "output port", &port, 0);
    else
	ok = cp_va_parse(words, r, errh,
			 cpIPAddressOrPrefix, "routing prefix", &dst, &mask,
			 cpIPAddress, "gateway address", &gw,
			 cpInteger, "output port", &port, 0);

    if (ok >= 0 && (port < 0 || port >= r->noutputs()))
	ok = errh->error("output port out of range");
    if (ok >= 0)
	ok = r->add_route(dst, mask, gw, port, errh);
    return ok;
}

int
IPRouteTable::remove_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    IPRouteTable *r = static_cast<IPRouteTable *>(e);

    Vector<String> words;
    cp_spacevec(conf, words);

    IPAddress a, mask, gw;
    int port = -1, ok;

    if (words.size() <= 2)
	ok = cp_va_parse(words, r, errh,
			 cpIPAddressOrPrefix, "routing prefix", &a, &mask,
			 cpOptional,
			 cpInteger, "output port", &port, 0);
    else
	ok = cp_va_parse(words, r, errh,
			 cpIPAddressOrPrefix, "routing prefix", &a, &mask,
			 cpIPAddress, "gateway address", &gw,
			 cpInteger, "output port", &port, 0);

    if (ok >= 0)
	ok = r->remove_route(a, mask, gw, port, errh);
    return ok;
}

int
IPRouteTable::ctrl_handler(const String &conf_in, Element *e, void *thunk, ErrorHandler *errh)
{
    String conf = conf_in;
    String first_word = cp_pop_spacevec(conf);
    if (first_word == "add")
	return add_route_handler(conf, e, thunk, errh);
    else if (first_word == "remove")
	return remove_route_handler(conf, e, thunk, errh);
    else
	return errh->error("bad command, should be `add' or `remove'");
}

String
IPRouteTable::table_handler(Element *e, void *)
{
    IPRouteTable *r = static_cast<IPRouteTable*>(e);
    return r->dump_routes();
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(IPRouteTable)
