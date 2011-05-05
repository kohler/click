/*
 * vlanencap.{cc,hh} -- encapsulates packet in Ethernet header
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include "vlanencap.hh"
CLICK_DECLS

VLANEncap::VLANEncap()
{
}

VLANEncap::~VLANEncap()
{
}

int
VLANEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint16_t vid;
    bool vid_specified;
    int tci_anno = -1;
    if (Args(conf, this, errh)
	.read("TCI_ANNO", AnnoArg(2), tci_anno)
	.read("VID", vid).read_status(vid_specified)
        .complete() < 0)
        return -1;

    if ((tci_anno < 0) == !vid_specified)
	return errh->error("must set exactly one of VID or TCI_ANNO");

    _tci_anno = tci_anno;
    _vid = htons((uint16_t)vid);
    return 0;
}

Packet *
VLANEncap::simple_action(Packet *p)
{
    assert(!p->mac_header() || p->mac_header() == p->data());
    uint16_t tci = 0;
    if (_tci_anno != 255)
	tci = p->anno_u16(_tci_anno);
    else
	tci = _vid;

    if (tci & htons(0xFFF)) {
	WritablePacket *q = p->push(4);
	if (!q)
	    return 0;
	memmove(q->data(), q->data() + 4, 12);
	click_ether_vlan *vlan = (click_ether_vlan *) q->data();
	vlan->ether_vlan_proto = htons(ETHERTYPE_8021Q);
	vlan->ether_vlan_tci = tci;
	q->set_mac_header(q->data(), 18);
	return q;
    } else {
	p->set_mac_header(p->data(), 14);
	return p;
    }
}

void
VLANEncap::add_handlers()
{
    add_net_order_data_handlers("vid", Handler::OP_READ | Handler::OP_WRITE, &_vid);
    add_read_handler("config", read_param, (void *)0, Handler::CALM);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(VLANEncap)
