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
#include <click/straccum.hh>
#include "iproutetable.hh"
CLICK_DECLS

StringAccum&
operator<<(StringAccum& sa, const IPRoute& r)
{
    sa << r.addr.unparse_with_mask(r.mask);
    if (r.gw)
	sa << ' ' << r.gw;
    sa << ' ' << r.port;
    return sa;
}

String
IPRoute::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}


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
    IPRoute r;
    for (int i = 0; i < conf.size(); i++) {
	Vector<String> words;
	cp_spacevec(conf[i], words);

	bool ok = false;
	if ((words.size() == 2 || words.size() == 3)
	    && cp_ip_prefix(words[0], &r.addr, &r.mask, true, this)
	    && cp_integer(words.back(), &r.port)) {
	    if (words.size() == 3)
		ok = cp_ip_address(words[1], &r.gw, this);
	    else {
		r.gw = IPAddress();
		ok = true;
	    }
	}

	if (ok && r.port >= 0 && r.port < noutputs()) {
	    r.addr &= r.mask;
	    (void) add_route(r, errh);
	} else
	    errh->error("argument %d should be `DADDR/MASK [GATEWAY] OUTPUT'", i+1);
    }

    return (errh->nerrors() != before ? -1 : 0);
}

int
IPRouteTable::add_route(const IPRoute&, ErrorHandler *errh)
{
    // by default, cannot add routes
    return errh->error("cannot add routes to this routing table");
}

int
IPRouteTable::remove_route(const IPRoute&, ErrorHandler *errh)
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
    IPRouteTable *table = static_cast<IPRouteTable *>(e);

    Vector<String> words;
    cp_spacevec(conf, words);

    IPRoute r;
    int ok;

    if (words.size() == 2)
	ok = cp_va_parse(words, table, errh,
			 cpIPAddressOrPrefix, "routing prefix", &r.addr, &r.mask,
			 cpInteger, "output port", &r.port, cpEnd);
    else
	ok = cp_va_parse(words, table, errh,
			 cpIPAddressOrPrefix, "routing prefix", &r.addr, &r.mask,
			 cpIPAddress, "gateway address", &r.gw,
			 cpInteger, "output port", &r.port, cpEnd);

    if (ok >= 0 && (r.port < 0 || r.port >= table->noutputs()))
	ok = errh->error("output port out of range");
    if (ok >= 0) {
	r.addr &= r.mask;
	ok = table->add_route(r, errh);
    }
    return ok;
}

int
IPRouteTable::remove_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    IPRouteTable *table = static_cast<IPRouteTable *>(e);

    Vector<String> words;
    cp_spacevec(conf, words);

    IPRoute r;
    r.port = -1;
    int ok;

    if (words.size() <= 2)
	ok = cp_va_parse(words, table, errh,
			 cpIPAddressOrPrefix, "routing prefix", &r.addr, &r.mask,
			 cpOptional,
			 cpInteger, "output port", &r.port, cpEnd);
    else
	ok = cp_va_parse(words, table, errh,
			 cpIPAddressOrPrefix, "routing prefix", &r.addr, &r.mask,
			 cpIPAddress, "gateway address", &r.gw,
			 cpInteger, "output port", &r.port, cpEnd);

    if (ok >= 0 && (r.port < -1 || r.port >= table->noutputs()))
	ok = errh->error("output port out of range");
    if (ok >= 0) {
	r.addr &= r.mask;
	ok = table->remove_route(r, errh);
    }
    return ok;
}

int
IPRouteTable::ctrl_handler(const String &conf_in, Element *e, void *thunk, ErrorHandler *errh)
{
    String conf = cp_uncomment(conf_in);
    const char* s = conf.begin(), *end = conf.end();
    int retval = 0;
    while (s < end) {
	const char* nl = find(s, end, '\n');
	String line = conf.substring(s, nl);
	String first_word = cp_pop_spacevec(line);
	int r;
	if (first_word == "add")
	    r = add_route_handler(line, e, thunk, errh);
	else if (first_word == "remove")
	    r = remove_route_handler(line, e, thunk, errh);
	else if (!first_word)
	    r = 0;
	else
	    r = errh->error("bad command, should be 'add' or 'remove'");
	if (r < 0)
	    retval = r;
	s = nl + 1;
    }
    return retval;
}

String
IPRouteTable::table_handler(Element *e, void *)
{
    IPRouteTable *r = static_cast<IPRouteTable*>(e);
    return r->dump_routes();
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(IPRouteTable)
