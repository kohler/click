// -*- c-basic-offset: 4; related-file-name: "../include/click/routervisitor.hh" -*-
/*
 * routervisitor.{cc,hh} -- element filters
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
#include <click/routervisitor.hh>
#include <click/router.hh>
CLICK_DECLS

/** @file routervisitor.hh
 * @brief Router configuration visitors for finding elements and ports.
 */

bool
RouterVisitor::visit(Element *, bool, int, Element *, int, int)
{
    return true;
}

ElementTracker::ElementTracker(Router *router)
    : _reached(router->nelements(), false)
{
}

bool
ElementCastTracker::visit(Element *e, bool, int, Element *, int, int)
{
    if (e->cast(_name.c_str())) {
	insert(e);
	return false;
    } else
	return true;
}

bool
ElementNeighborhoodTracker::visit(Element *e, bool, int,
				  Element *, int, int distance)
{
    if (distance <= _diameter)
	insert(e);
    return distance < _diameter;
}

CLICK_ENDDECLS
