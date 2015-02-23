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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
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
    String tci_word;
    _ethertype = ETHERTYPE_8021Q;
    int tci = -1, id = 0, pcp = 0, native_vlan = 0;
    if (Args(conf, this, errh)
	.read_p("VLAN_TCI", WordArg(), tci_word)
	.read_p("VLAN_PCP", BoundedIntArg(0, 7), pcp)
	.read("VLAN_ID", BoundedIntArg(0, 0xFFF), id)
	.read("NATIVE_VLAN", BoundedIntArg(-1, 0xFFF), native_vlan)
	.read("ETHERTYPE", _ethertype)
	.complete() < 0)
	return -1;
    if (tci_word && !tci_word.equals("ANNO", 4)
	&& Args(this, errh).push_back(tci_word)
	   .read_p("VLAN_TCI", BoundedIntArg(0, 0xFFFF), tci)
	   .complete() < 0)
	return -1;
    _vlan_tci = htons((tci >= 0 ? tci : id) | (pcp << 13));
    _use_anno = tci_word.equals("ANNO", 4);
    _native_vlan = (native_vlan >= 0 ? htons(native_vlan) : -1);
    _ethertype = htons(_ethertype);
    return 0;
}

Packet *
VLANEncap::simple_action(Packet *p)
{
    assert(!p->mac_header() || p->mac_header() == p->data());
    uint16_t tci = _vlan_tci;
    if (_use_anno)
	tci = VLAN_TCI_ANNO(p);
    if ((tci & htons(0xFFF)) == _native_vlan) {
	p->set_mac_header(p->data(), sizeof(click_ether));
	return p;
    } else if (WritablePacket *q = p->push(4)) {
	memmove(q->data(), q->data() + 4, 12);
	click_ether_vlan *vlan = reinterpret_cast<click_ether_vlan *>(q->data());
	vlan->ether_vlan_proto = _ethertype;
	vlan->ether_vlan_tci = tci;
	q->set_mac_header(q->data(), sizeof(vlan));
	return q;
    } else
	return 0;
}

String
VLANEncap::read_handler(Element *e, void *user_data)
{
    VLANEncap *ve = static_cast<VLANEncap *>(e);
    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_config: {
	StringAccum sa;
	if (ve->_use_anno)
	    sa << "ANNO";
	else
	    sa << "VLAN_ID " << (ntohs(ve->_vlan_tci) & 0xFFF)
	       << ", VLAN_PCP " << ((ntohs(ve->_vlan_tci) >> 13) & 7);
	if (ve->_native_vlan != 0)
	    sa << ", NATIVE_VLAN " << ntohs(ve->_native_vlan);
	return sa.take_string();
    }
    case h_vlan_tci:
	if (ve->_use_anno)
	    return String::make_stable("ANNO", 4);
	else
	    return String(ntohs(ve->_vlan_tci));
    }
    return String();
}

void
VLANEncap::add_handlers()
{
    add_read_handler("config", read_handler, h_config);
    add_read_handler("vlan_tci", read_handler, h_vlan_tci);
    add_write_handler("vlan_tci", reconfigure_keyword_handler, "0 VLAN_TCI");
    add_read_handler("vlan_id", read_keyword_handler, "VLAN_ID");
    add_write_handler("vlan_id", reconfigure_keyword_handler, "VLAN_ID");
    add_read_handler("vlan_pcp", read_keyword_handler, "VLAN_PCP");
    add_write_handler("vlan_pcp", reconfigure_keyword_handler, "VLAN_PCP");
    add_read_handler("native_vlan", read_keyword_handler, "NATIVE_VLAN");
    add_write_handler("native_vlan", reconfigure_keyword_handler, "NATIVE_VLAN");
    add_read_handler("ethertype", read_keyword_handler, "ETHERTYPE");
    add_write_handler("ethertype", reconfigure_keyword_handler, "ETHERTYPE");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(VLANEncap)
