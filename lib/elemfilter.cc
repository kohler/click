// -*- c-basic-offset: 4; related-file-name: "../include/click/elemfilter.hh" -*-
/*
 * elemfilter.{cc,hh} -- element filters
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2008 Regents of the University of California
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
#include <click/elemfilter.hh>
CLICK_DECLS

/** @file elemfilter.hh
 * @brief Filter predicates for elements and ports.
 */

/** @class ElementFilter
 * @brief Base class for filter predicates for elements and ports.
 *
 * @deprecated This class is deprecated.  Use RouterVisitor instead.
 *
 * ElementFilter objects are used to search the router configuration graph for
 * matching elements.  They are usually passed to the
 * Router::downstream_elements() and Router::upstream_elements() functions.
 */

/** @class CastElementFilter
 * @brief Cast-based filter predicate for elements.
 *
 * @deprecated This class is deprecated.  Use ElementCastTracker instead.
 *
 * This ElementFilter type matches elements that match a given cast type.
 * The constructor's string is passed to Element::cast(); the filter matches
 * an element iff the result is non-null.
 */

bool
ElementFilter::check_match(Element *, bool, int)
{
    return false;
}

void
ElementFilter::filter(Vector<Element *> &es)
{
    Element **o = es.begin();
    for (Element **i = o; i != es.end(); ++i)
	if (check_match(*i, false, -1))
	    *o++ = *i;
    es.resize(o - es.begin());
}

CastElementFilter::CastElementFilter(const String &name)
    : _name(name)
{
}

bool
CastElementFilter::check_match(Element *e, bool, int)
{
    return e->cast(_name.c_str()) != 0;
}

CLICK_ENDDECLS
