/*
 * ethervlanencap.{cc,hh} -- encapsulates packet in Ethernet header
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2010 Intel Corporation
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
#include "ethervlanencap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

EtherVLANEncap::EtherVLANEncap()
{
}

EtherVLANEncap::~EtherVLANEncap()
{
}

int
EtherVLANEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    click_ether_vlan ethh;
    String vlan_word;
    int vlan = 0, vlan_pcp = 0, native_vlan = -1;
    uint16_t ether_vlan_encap_proto;
    ethh.ether_vlan_proto = htons(ETHERTYPE_8021Q);
    if (Args(conf, this, errh)
	.read_mp("ETHERTYPE", ether_vlan_encap_proto)
	.read_mp_with("SRC", EtherAddressArg(), ethh.ether_shost)
	.read_mp_with("DST", EtherAddressArg(), ethh.ether_dhost)
	.read_mp("VLAN", WordArg(), vlan_word)
	.read_p("VLAN_PCP", vlan_pcp)
	.read("NATIVE_VLAN", native_vlan)
	.complete() < 0)
	return -1;
    if (!vlan_word.equals("ANNO", 4)
	&& (!IntArg().parse(vlan_word, vlan) || vlan < 0 || vlan >= 0x0FFF))
	return errh->error("bad VLAN");
    if (vlan_pcp < 0 || vlan_pcp > 0x7)
	return errh->error("bad VLAN_PCP");
    if (native_vlan >= 0x0FFF)
	return errh->error("bad NATIVE_VLAN");
    ethh.ether_vlan_tci = htons(vlan | (vlan_pcp << 13));
    ethh.ether_vlan_encap_proto = htons(ether_vlan_encap_proto);
    _ethh = ethh;
    _use_anno = vlan_word.equals("ANNO", 4);
    _use_native_vlan = native_vlan >= 0;
    _native_vlan = htons(native_vlan);
    return 0;
}

Packet *
EtherVLANEncap::smaction(Packet *p)
{
    if (_use_anno) {
	if (_use_native_vlan
	    && (VLAN_ANNO(p) & htons(0x0FFF)) == _native_vlan) {
	    if (WritablePacket *q = p->push_mac_header(sizeof(click_ether))) {
		memcpy(q->data(), &_ethh, 12);
		q->ether_header()->ether_type = _ethh.ether_vlan_encap_proto;
		return q;
	    } else
		return 0;
	} else
	    _ethh.ether_vlan_tci = VLAN_ANNO(p);
    }
    if (WritablePacket *q = p->push_mac_header(sizeof(click_ether_vlan))) {
	memcpy(q->data(), &_ethh, sizeof(click_ether_vlan));
	return q;
    } else
	return 0;
}

void
EtherVLANEncap::push(int, Packet *p)
{
    if (Packet *q = smaction(p))
	output(0).push(q);
}

Packet *
EtherVLANEncap::pull(int)
{
    if (Packet *p = input(0).pull())
	return smaction(p);
    else
	return 0;
}

String
EtherVLANEncap::read_handler(Element *e, void *user_data)
{
    EtherVLANEncap *eve = static_cast<EtherVLANEncap *>(e);
    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_vlan:
	if (eve->_use_anno)
	    return String::make_stable("ANNO", 4);
	else
	    return String(ntohs(eve->_ethh.ether_vlan_tci) & 0x0FFF);
    case h_vlan_pcp:
	return String((ntohs(eve->_ethh.ether_vlan_tci) >> 13) & 0x7);
    }
    return String();
}

void
EtherVLANEncap::add_handlers()
{
    add_data_handlers("src", Handler::h_read, reinterpret_cast<EtherAddress *>(&_ethh.ether_shost));
    add_write_handler("src", reconfigure_keyword_handler, "1 SRC");
    add_data_handlers("dst", Handler::h_read, reinterpret_cast<EtherAddress *>(&_ethh.ether_dhost));
    add_write_handler("dst", reconfigure_keyword_handler, "2 DST");
    add_net_order_data_handlers("ethertype", Handler::h_read, &_ethh.ether_vlan_encap_proto);
    add_write_handler("ethertype", reconfigure_keyword_handler, "0 ETHERTYPE");
    add_read_handler("vlan", read_handler, h_vlan);
    add_write_handler("vlan", reconfigure_keyword_handler, "3 VLAN");
    add_read_handler("vlan_pcp", read_handler, h_vlan_pcp);
    add_write_handler("vlan_pcp", reconfigure_keyword_handler, "4 VLAN_PCP");
    add_net_order_data_handlers("native_vlan", Handler::h_read, &_native_vlan);
    add_write_handler("native_vlan", reconfigure_keyword_handler, "NATIVE_VLAN");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EtherVLANEncap EtherVLANEncap-EtherVlanEncap)
