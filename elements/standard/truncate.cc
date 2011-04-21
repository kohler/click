// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * truncate.{cc,hh} -- limits packet length
 * Eddie Kohler
 *
 * Copyright (c) 2004 Regents of the University of California
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
#include "truncate.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

Truncate::Truncate()
{
}

Truncate::~Truncate()
{
}

int
Truncate::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned short nbytes;
    bool extra_length = true;
    if (Args(conf, this, errh)
	.read_mp("LENGTH", nbytes)
	.read("EXTRA_LENGTH", extra_length)
	.complete() < 0)
	return -1;
    _nbytes = (nbytes << 1) + extra_length;
    return 0;
}

Packet *
Truncate::simple_action(Packet *p)
{
    unsigned nbytes = _nbytes >> 1;
    if (p->length() > nbytes) {
	nbytes = p->length() - nbytes;
	if (_nbytes & 1)
	    SET_EXTRA_LENGTH_ANNO(p, EXTRA_LENGTH_ANNO(p) + nbytes);
        p->take(nbytes);
    }
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Truncate)
ELEMENT_MT_SAFE(Truncate)
