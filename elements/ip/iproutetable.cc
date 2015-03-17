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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include "iproutetable.hh"
CLICK_DECLS

bool
cp_ip_route(String s, IPRoute *r_store, bool remove_route, Element *context)
{
    IPRoute r;
    if (!IPPrefixArg(true).parse(cp_shift_spacevec(s), r.addr, r.mask, context))
	return false;
    r.addr &= r.mask;

    String word = cp_shift_spacevec(s);
    if (word == "-")
	/* null gateway; do nothing */;
    else if (IPAddressArg().parse(word, r.gw, context))
	/* do nothing */;
    else
	goto two_words;

    word = cp_shift_spacevec(s);
  two_words:
    if (IntArg().parse(word, r.port) || (!word && remove_route))
	if (!cp_shift_spacevec(s)) { // nothing left
	    *r_store = r;
	    return true;
	}

    return false;
}

StringAccum&
IPRoute::unparse(StringAccum& sa, bool tabs) const
{
    int l = sa.length();
    char tab = (tabs ? '\t' : ' ');
    sa << addr.unparse_with_mask(mask) << tab;
    if (sa.length() < l + 17 && tabs)
	sa << '\t';
    l = sa.length();
    if (gw)
	sa << gw << tab;
    else
	sa << '-' << tab;
    if (sa.length() < l + 9 && tabs)
	sa << '\t';
    if (!real())
	sa << "-1";
    else
	sa << port;
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
    int r = 0, r1, eexist = 0;
    IPRoute route;
    for (int i = 0; i < conf.size(); i++) {
	if (!cp_ip_route(conf[i], &route, false, this)) {
	    errh->error("argument %d should be %<ADDR/MASK [GATEWAY] OUTPUT%>", i+1);
	    r = -EINVAL;
	} else if (route.port < 0 || route.port >= noutputs()) {
	    errh->error("argument %d bad OUTPUT", i+1);
	    r = -EINVAL;
	} else if ((r1 = add_route(route, false, 0, errh)) < 0) {
	    if (r1 == -EEXIST)
		++eexist;
	    else
		r = r1;
	}
    }
    if (eexist)
	errh->warning("%d %s replaced by later versions", eexist, eexist > 1 ? "routes" : "route");
    return r;
}

int
IPRouteTable::add_route(const IPRoute&, bool, IPRoute*, ErrorHandler *errh)
{
    // by default, cannot add routes
    return errh->error("cannot add routes to this routing table");
}

int
IPRouteTable::remove_route(const IPRoute&, IPRoute*, ErrorHandler *errh)
{
    // by default, cannot remove routes
    return errh->error("cannot delete routes from this routing table");
}

int
IPRouteTable::lookup_route(IPAddress, IPAddress&) const
{
    return -1;			// by default, route lookups fail
}

String
IPRouteTable::dump_routes()
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
	static int complained = 0;
	if (++complained <= 5)
	    click_chatter("IPRouteTable: no route for %s", p->dst_ip_anno().unparse().c_str());
	p->kill();
    }
}


int
IPRouteTable::run_command(int command, const String &str, Vector<IPRoute>* old_routes, ErrorHandler *errh)
{
    IPRoute route, old_route;
    if (!cp_ip_route(str, &route, command == CMD_REMOVE, this))
	return errh->error("expected %<ADDR/MASK [GATEWAY%s%>", (command == CMD_REMOVE ? " OUTPUT]" : "] OUTPUT"));
    else if (route.port < (command == CMD_REMOVE ? -1 : 0)
	     || route.port >= noutputs())
	return errh->error("bad OUTPUT");

    int r, before = errh->nerrors();
    if (command == CMD_ADD)
	r = add_route(route, false, &old_route, errh);
    else if (command == CMD_SET)
	r = add_route(route, true, &old_route, errh);
    else
	r = remove_route(route, &old_route, errh);

    // save old route if in a transaction
    if (r >= 0 && old_routes) {
	if (old_route.port < 0) { // must come from add_route
	    old_route = route;
	    old_route.extra = CMD_ADD;
	} else
	    old_route.extra = command;
	old_routes->push_back(old_route);
    }

    // report common errors
    if (r == -EEXIST && errh->nerrors() == before)
	errh->error("conflict with existing route %<%s%>", old_route.unparse().c_str());
    if (r == -ENOENT && errh->nerrors() == before)
	errh->error("route %<%s%> not found", route.unparse().c_str());
    if (r == -ENOMEM && errh->nerrors() == before)
	errh->error("no memory to store route %<%s%>", route.unparse().c_str());
    return r;
}


int
IPRouteTable::add_route_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
    IPRouteTable *table = static_cast<IPRouteTable *>(e);
    return table->run_command((thunk ? CMD_SET : CMD_ADD), conf, 0, errh);
}

int
IPRouteTable::remove_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    IPRouteTable *table = static_cast<IPRouteTable *>(e);
    return table->run_command(CMD_REMOVE, conf, 0, errh);
}

int
IPRouteTable::ctrl_handler(const String &conf_in, Element *e, void *, ErrorHandler *errh)
{
    IPRouteTable *table = static_cast<IPRouteTable *>(e);
    String conf = cp_uncomment(conf_in);
    const char* s = conf.begin(), *end = conf.end();

    Vector<IPRoute> old_routes;
    int r = 0;

    while (s < end) {
	const char* nl = find(s, end, '\n');
	String line = conf.substring(s, nl);

	String first_word = cp_shift_spacevec(line);
	int command;
	if (first_word == "add")
	    command = CMD_ADD;
	else if (first_word == "remove")
	    command = CMD_REMOVE;
	else if (first_word == "set")
	    command = CMD_SET;
	else if (!first_word)
	    continue;
	else {
	    r = errh->error("bad command %<%#s%>", first_word.c_str());
	    goto rollback;
	}

	if ((r = table->run_command(command, line, &old_routes, errh)) < 0)
	    goto rollback;

	s = nl + 1;
    }
    return 0;

  rollback:
    while (old_routes.size()) {
	const IPRoute& rt = old_routes.back();
	if (rt.extra == CMD_REMOVE)
	    table->add_route(rt, false, 0, errh);
	else if (rt.extra == CMD_ADD)
	    table->remove_route(rt, 0, errh);
	else
	    table->add_route(rt, true, 0, errh);
	old_routes.pop_back();
    }
    return r;
}

String
IPRouteTable::table_handler(Element *e, void *)
{
    IPRouteTable *r = static_cast<IPRouteTable*>(e);
    return r->dump_routes();
}

int
IPRouteTable::lookup_handler(int, String& s, Element* e, const Handler*, ErrorHandler* errh)
{
    IPRouteTable *table = static_cast<IPRouteTable*>(e);
    IPAddress a;
    if (IPAddressArg().parse(s, a, table)) {
	IPAddress gw;
	int port = table->lookup_route(a, gw);
	if (gw)
	    s = String(port) + " " + gw.unparse();
	else
	    s = String(port);
	return 0;
    } else
	return errh->error("expected IP address");
}

void
IPRouteTable::add_handlers()
{
    add_write_handler("add", add_route_handler, 0);
    add_write_handler("set", add_route_handler, 1);
    add_write_handler("remove", remove_route_handler);
    add_write_handler("ctrl", ctrl_handler);
    add_read_handler("table", table_handler, 0, Handler::f_expensive);
    set_handler("lookup", Handler::f_read | Handler::f_read_param, lookup_handler);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(IPRouteTable)
