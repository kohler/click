/*
 * hostetherfilter.{cc,hh} -- Discard packets not for this host.
 * Robert Morris
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
#include "hostetherfilter.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
CLICK_DECLS

HostEtherFilter::HostEtherFilter()
{
}

HostEtherFilter::~HostEtherFilter()
{
}

int
HostEtherFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool drop_own = false, drop_other = true;
    uint32_t offset = 0;
    if (Args(conf, this, errh)
	.read_mp("ETHER", _addr)
	.read_p("DROP_OWN", drop_own)
	.read_p("DROP_OTHER", drop_other)
	.read("OFFSET", offset)
	.complete() < 0)
	return -1;
    _offset = offset;
    _drop_own = drop_own;
    _drop_other = drop_other;
    return 0;
}

Packet *
HostEtherFilter::simple_action(Packet *p)
{
    const click_ether *e = (const click_ether *) (p->data() + _offset);
    const unsigned short *daddr = (const unsigned short *)e->ether_dhost;

    if (_drop_own && memcmp(e->ether_shost, _addr.data(), 6) == 0) {
	checked_output_push(1, p);
	p = 0;
    } else if (memcmp(e->ether_dhost, _addr.data(), 6) == 0) {
	p->set_packet_type_anno(Packet::HOST);
    } else if (daddr[0] == 0xFFFF && daddr[1] == 0xFFFF && daddr[2] == 0xFFFF) {
	p->set_packet_type_anno(Packet::BROADCAST);
    } else if (e->ether_dhost[0] & 0x01) {
	p->set_packet_type_anno(Packet::MULTICAST);
    } else {
	p->set_packet_type_anno(Packet::OTHERHOST);
	if (_drop_other) {
	    checked_output_push(1, p);
	    p = 0;
	}
    }
    return p;
}

void
HostEtherFilter::add_handlers()
{
    add_data_handlers("ether", Handler::OP_READ | Handler::OP_WRITE, &_addr);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(HostEtherFilter)
