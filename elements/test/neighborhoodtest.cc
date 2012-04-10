// -*- c-basic-offset: 4 -*-
/*
 * neighborhoodtest.{cc,hh} -- test ElementNeighborhoodTracker
 * Eddie Kohler
 *
 * Copyright (c) 2009 Meraki, Inc.
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
#include "neighborhoodtest.hh"
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
CLICK_DECLS

NeighborhoodTest::NeighborhoodTest()
{
}

Packet *
NeighborhoodTest::simple_action(Packet *p)
{
    p->kill();
    return 0;
}

int
NeighborhoodTest::handler(int, String &data, Element *element,
			  const Handler *handler, ErrorHandler *errh)
{
    cp_uncomment(data);
    int diameter = 1;
    if (data && !IntArg().parse(data, diameter))
	return errh->error("syntax error");
    ElementNeighborhoodTracker tracker(element->router(), diameter);
    intptr_t port = (intptr_t) handler->write_user_data();
    if (handler->read_user_data())
	element->router()->visit_downstream(element, port, &tracker);
    else
	element->router()->visit_upstream(element, port, &tracker);
    StringAccum sa;
    for (int i = 0; i < tracker.size(); ++i)
	sa << tracker[i]->name() << '\n';
    data = sa.take_string();
    return 0;
}

void
NeighborhoodTest::add_handlers()
{
    for (int i = -1; i < ninputs(); ++i)
	set_handler("upstream" + (i < 0 ? String() : String(i)), Handler::OP_READ | Handler::READ_PARAM, handler, 0, i);
    for (int o = -1; o < noutputs(); ++o)
	set_handler("downstream" + (o < 0 ? String() : String(o)), Handler::OP_READ | Handler::READ_PARAM, handler, 1, o);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NeighborhoodTest)
