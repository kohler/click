// -*- c-basic-offset: 4; related-file-name: "../include/click/elemfilter.hh" -*-
/*
 * elemfilter.{cc,hh} -- element filters
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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

void
ElementFilter::filter(Vector<Element *> &v)
{
    Vector<Element *> nv;
    for (int i = 0; i < v.size(); i++)
	if (match(v[i], -1))
	    nv.push_back(v[i]);
    v = nv;
}


CastElementFilter::CastElementFilter(const String &what)
    : _what(what)
{
}

bool
CastElementFilter::check_match(Element *e, int)
{
    return e->cast(_what) != 0;
}
