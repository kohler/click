/*
 * storeetheraddress.{cc,hh} -- element stores Ethernet address into packet
 * Eddie Kohler
 *
 * Copyright (c) 2008-2012 Meraki, Inc.
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
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

int
StoreEtherAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String off;
    bool address_specified, anno_specified;
    int anno;
    if (Args(conf, this, errh)
	.read_p("ADDR", _address).read_status(address_specified)
	.read_mp("OFFSET", WordArg(), off)
	.read("ANNO", AnnoArg(6), anno).read_status(anno_specified)
	.complete() < 0)
	return -1;

    if (!(address_specified ^ anno_specified))
	return errh->error("must specify exactly one of ADDR/ANNO");

    uint32_t offset = 0;
    if (off.lower() == "src")
	offset = 6;
    else if (off.lower() == "dst")
	offset = 0;
    else if (!IntArg().parse(off, offset) || offset + 6 < 6)
	return errh->error("type mismatch: bad OFFSET");

    _offset = offset;
    _use_anno = anno_specified;
    _anno = anno;
    return 0;
}

Packet *
StoreEtherAddress::simple_action(Packet *p)
{
    if (_offset + 6 <= p->length()) {
	if (WritablePacket *q = p->uniqueify()) {
	    const uint8_t *addr = _use_anno ? q->anno_u8() + _anno : _address.data();
	    memcpy(q->data() + _offset, addr, 6);
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
