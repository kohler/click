/*
 * storeipaddress.{cc,hh} -- element stores Ethernet address into packet
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
#include "storeetheraddress.hh"
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

StoreEtherAddress::StoreEtherAddress()
{
}

StoreEtherAddress::~StoreEtherAddress()
{
}

int
StoreEtherAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String off;
    if (cp_va_kparse(conf, this, errh,
		     "ADDR", cpkP+cpkM, cpEtherAddress, &_address,
		     "OFFSET", cpkP+cpkM, cpWord, &off,
		     cpEnd) < 0)
	return -1;
    if (off.lower() == "src")
	_offset = 6;
    else if (off.lower() == "dst")
	_offset = 0;
    else if (!cp_integer(off, &_offset))
	return errh->error("type mismatch: bad OFFSET");
    return 0;
}

Packet *
StoreEtherAddress::simple_action(Packet *p)
{
    if (_offset + 6 <= p->length()) {
	if (WritablePacket *q = p->uniqueify()) {
	    memcpy(q->data() + _offset, &_address, 6);
	    return q;
	} else
	    return 0;
    } else {
	checked_output_push(1, p);
	return 0;
    }
}

void
StoreEtherAddress::add_handlers()
{
    add_data_handlers("addr", Handler::OP_READ | Handler::OP_WRITE, &_address);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StoreEtherAddress)
ELEMENT_MT_SAFE(StoreEtherAddress)
