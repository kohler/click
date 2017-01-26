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

int
Truncate::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _extra_anno = true;
    return Args(conf, this, errh)
	.read_mp("LENGTH", _nbytes)
	.read("EXTRA_LENGTH", _extra_anno)
	.complete();
}

Packet *
Truncate::simple_action(Packet *p)
{
    if (p->length() > _nbytes) {
	unsigned nbytes = p->length() - _nbytes;
	if (_extra_anno)
	    SET_EXTRA_LENGTH_ANNO(p, EXTRA_LENGTH_ANNO(p) + nbytes);
        p->take(nbytes);
    }
    return p;
}

void
Truncate::add_handlers()
{
    add_data_handlers("nbytes", Handler::OP_READ | Handler::OP_WRITE, &_nbytes);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Truncate)
ELEMENT_MT_SAFE(Truncate)
