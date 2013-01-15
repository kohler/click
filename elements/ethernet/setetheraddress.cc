/*
 * setetheraddress.{cc,hh} -- sets annotation area
 * to a particular ethernet address
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "setetheraddress.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

int
SetEtherAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("ADDR", _addr)
	.read_mp("ANNO", AnnoArg(6), _anno)
	.complete();
}

Packet *
SetEtherAddress::simple_action(Packet *p)
{
    uint8_t *anno_p = p->anno_u8() + _anno;
    memcpy(anno_p, _addr.data(), 6);
    return p;
}

void
SetEtherAddress::add_handlers()
{
    add_data_handlers("addr", Handler::OP_READ | Handler::OP_WRITE, &_addr);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetEtherAddress)
ELEMENT_MT_SAFE(SetEtherAddress)
