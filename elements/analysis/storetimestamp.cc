// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * storetimestamp.{cc,hh} -- element stores timestamp in packet data
 * Eddie Kohler
 *
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/error.hh>
#include "storetimestamp.hh"
#include <click/args.hh>
#include <click/straccum.hh>
CLICK_DECLS

StoreTimestamp::StoreTimestamp()
{
}

StoreTimestamp::~StoreTimestamp()
{
}

int
StoreTimestamp::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _offset = -1;
    bool tail = false;
    if (Args(conf, this, errh)
	.read_p("OFFSET", _offset)
	.read("TAIL", tail)
	.complete() < 0)
	return -1;
    if (_offset >= 0 ? tail : !tail)
	return errh->error("supply exactly one of 'OFFSET' and 'TAIL'");
    return 0;
}

Packet *
StoreTimestamp::simple_action(Packet *p)
{
    int offset = (_offset < 0 ? (int) p->length() : _offset);
    int delta = offset + 8 - p->length();
    if (WritablePacket *q = p->put(delta < 0 ? 0 : delta)) {
	memcpy(q->data() + offset, &q->timestamp_anno(), 8);
	return q;
    } else
	return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StoreTimestamp)
