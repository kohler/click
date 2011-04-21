// -*- c-basic-offset: 4 -*-
/*
 * ip6 handler kludge by Marko Zec
 *
 * originates from iproutetable.{cc,hh} by Benjie Chen, Eddie Kohler
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
#include <click/ip6address.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "ip6routetable.hh"
CLICK_DECLS

void *
IP6RouteTable::cast(const char *name)
{
    if (strcmp(name, "IPRouteTable") == 0)
	return (void *)this;
    else
	return Element::cast(name);
}

int
IP6RouteTable::add_route(IP6Address, IP6Address, IP6Address,
			 int, ErrorHandler *errh)
{
    // by default, cannot add routes
    return errh->error("cannot add routes to this routing table");
}

int
IP6RouteTable::remove_route(IP6Address, IP6Address, ErrorHandler *errh)
{
    // by default, cannot remove routes
    return errh->error("cannot delete routes from this routing table");
}

String
IP6RouteTable::dump_routes()
{
    return String();
}

int
IP6RouteTable::add_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    IP6RouteTable *r = static_cast<IP6RouteTable *>(e);

    Vector<String> words;
    cp_spacevec(conf, words);

    IP6Address dst, mask, gw;
    int port, ok;

    if (words.size() == 2)
        ok = Args(words, r, errh)
	    .read_mp("PREFIX", IP6PrefixArg(true), dst, mask)
	    .read_mp("PORT", port)
	    .complete();
    else
        ok = Args(words, r, errh)
	    .read_mp("PREFIX", IP6PrefixArg(true), dst, mask)
	    .read_mp("GATEWAY", gw)
	    .read_mp("PORT", port)
	    .complete();

    if (ok >= 0 && (port < 0 || port >= r->noutputs()))
        ok = errh->error("output port out of range");
    if (ok >= 0)
        ok = r->add_route(dst, mask, gw, port, errh);
    return ok;
}

int
IP6RouteTable::remove_route_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    IP6RouteTable *r = static_cast<IP6RouteTable *>(e);

    Vector<String> words;
    cp_spacevec(conf, words);

    IP6Address a, mask;
    int ok = 0;

    ok = Args(words, r, errh)
	.read_mp("PREFIX", IP6PrefixArg(true), a, mask)
	.complete();

    if (ok >= 0)
	ok = r->remove_route(a, mask, errh);
    return ok;
}

int
IP6RouteTable::ctrl_handler(const String &conf_in, Element *e, void *thunk, ErrorHandler *errh)
{
    String conf = conf_in;
    String first_word = cp_shift_spacevec(conf);
    if (first_word == "add")
	return add_route_handler(conf, e, thunk, errh);
    else if (first_word == "remove")
	return remove_route_handler(conf, e, thunk, errh);
    else
	return errh->error("bad command, should be `add' or `remove'");
}

String
IP6RouteTable::table_handler(Element *e, void *)
{
    IP6RouteTable *r = static_cast<IP6RouteTable*>(e);
    return r->dump_routes();
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(IP6RouteTable)
