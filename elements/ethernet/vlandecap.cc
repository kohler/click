/*
 * vlandecap.{cc,hh} -- deencapsulates packet in VLAN header
 *
 * Copyright (c) 2009-2011 Meraki, Inc.
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
#include "vlandecap.hh"
CLICK_DECLS

VLANDecap::VLANDecap()
{
}

VLANDecap::~VLANDecap()
{
}

int
VLANDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _anno = true;
    _ethertype = ETHERTYPE_8021Q;
    return Args(conf, this, errh)
	.read_p("ANNO", _anno)
        .read("ETHERTYPE", _ethertype)
        .complete();
}

Packet *
VLANDecap::simple_action(Packet *p)
{
    assert(!p->mac_header() || p->mac_header() == p->data());
    uint16_t tci = 0;
    const click_ether_vlan *vlan = reinterpret_cast<const click_ether_vlan *>(p->data());
    if (vlan->ether_vlan_proto == htons(_ethertype)) {
	tci = vlan->ether_vlan_tci;
	if (WritablePacket *q = p->uniqueify()) {
	    memmove(q->data() + 4, q->data(), 12);
	    q->pull(4);
	    p = q;
	} else
	    return 0;
    }
    p->set_mac_header(p->data(), sizeof(click_ether));
    if (_anno)
	SET_VLAN_TCI_ANNO(p, tci);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(VLANDecap)
