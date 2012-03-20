/*
 * hub.{cc,hh} -- element duplicates packets like a hub
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
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
#include "hub.hh"
CLICK_DECLS

Hub::Hub()
{
}

void
Hub::push(int port, Packet *p)
{
    int n = 0, nout = noutputs();
    for (int i = 0; i < nout; ++i) {
	Packet *q;
	if (i == port)
	    q = 0;
	else if (++n == nout - 1)
	    q = p;
	else
	    q = p->clone();
	if (q)
	    output(i).push(q);
    }
    if (n == 0)
	p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Hub)
