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
#include <click/straccum.hh>
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
    uint16_t ether_vlan_encap_proto;
    String tci_word;
    int tci = -1, id = 0, pcp = 0, native_vlan = 0;
    ethh.ether_vlan_proto = htons(ETHERTYPE_8021Q);
    if (Args(conf, this, errh)
	.read_mp("ETHERTYPE", ether_vlan_encap_proto)
	.read_mp("SRC", EtherAddressArg(), ethh.ether_shost)
	.read_mp("DST", EtherAddressArg(), ethh.ether_dhost)
	.read_p("VLAN_TCI", WordArg(), tci_word)
	.read_p("VLAN_PCP", BoundedIntArg(0, 7), pcp)
	.read("VLAN_ID", BoundedIntArg(0, 0xFFF), id)
	.read("NATIVE_VLAN", BoundedIntArg(-1, 0xFFF), native_vlan)
	.complete() < 0)
	return -1;
    if (tci_word && !tci_word.equals("ANNO", 4)
	&& Args(this, errh).push_back(tci_word)
	   .read_p("VLAN_TCI", BoundedIntArg(0, 0xFFFF), tci)
	   .complete() < 0)
	return -1;
    ethh.ether_vlan_tci = htons((tci >= 0 ? tci : id) | (pcp << 13));
    ethh.ether_vlan_encap_proto = htons(ether_vlan_encap_proto);
    _ethh = ethh;
    _use_anno = tci_word.equals("ANNO", 4);
    _native_vlan = (native_vlan >= 0 ? htons(native_vlan) : -1);
    return 0;
}

Packet *
EtherVLANEncap::smaction(Packet *p)
{
    if (_use_anno)
	_ethh.ether_vlan_tci = VLAN_TCI_ANNO(p);
    if ((_ethh.ether_vlan_tci & htons(0x0FFF)) == _native_vlan) {
	if (WritablePacket *q = p->push_mac_header(sizeof(click_ether))) {
	    memcpy(q->data(), &_ethh, 12);
	    q->ether_header()->ether_type = _ethh.ether_vlan_encap_proto;
	    return q;
	} else
	    return 0;
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
    case h_config: {
	StringAccum sa;
	sa << EtherAddress(eve->_ethh.ether_shost) << ", "
	   << EtherAddress(eve->_ethh.ether_dhost) << ", "
	   << ntohs(eve->_ethh.ether_vlan_encap_proto);
	if (eve->_use_anno)
	    sa << ", ANNO";
	else
	    sa << ", VLAN_ID " << (ntohs(eve->_ethh.ether_vlan_tci) & 0xFFF)
	       << ", VLAN_PCP " << ((ntohs(eve->_ethh.ether_vlan_tci) >> 13) & 7);
	if (eve->_native_vlan != 0)
	    sa << ", NATIVE_VLAN " << ntohs(eve->_native_vlan);
	return sa.take_string();
    }
    case h_vlan_tci:
	if (eve->_use_anno)
	    return String::make_stable("ANNO", 4);
	else
	    return String(ntohs(eve->_ethh.ether_vlan_tci));
    }
    return String();
}

void
EtherVLANEncap::add_handlers()
{
    add_read_handler("config", read_handler, h_config);
    add_data_handlers("src", Handler::h_read | Handler::h_write, reinterpret_cast<EtherAddress *>(&_ethh.ether_shost));
    add_data_handlers("dst", Handler::h_read | Handler::h_write, reinterpret_cast<EtherAddress *>(&_ethh.ether_dhost));
    add_net_order_data_handlers("ethertype", Handler::h_read | Handler::h_write, &_ethh.ether_vlan_encap_proto);
    add_read_handler("vlan_tci", read_handler, h_vlan_tci);
    add_write_handler("vlan_tci", reconfigure_keyword_handler, "3 VLAN_TCI");
    add_read_handler("vlan_id", read_keyword_handler, "VLAN_ID");
    add_write_handler("vlan_id", reconfigure_keyword_handler, "VLAN_ID");
    add_read_handler("vlan_pcp", read_keyword_handler, "VLAN_PCP");
    add_write_handler("vlan_pcp", reconfigure_keyword_handler, "VLAN_PCP");
    add_read_handler("native_vlan", read_keyword_handler, "NATIVE_VLAN");
    add_write_handler("native_vlan", reconfigure_keyword_handler, "NATIVE_VLAN");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EtherVLANEncap EtherVLANEncap-EtherVlanEncap)
