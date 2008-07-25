// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_link.{cc,hh} -- link layer IP summary dump unparsers
 * Eddie Kohler
 *
 * Copyright (c) 2008 Regents of the University of California
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

#include "ipsumdumpinfo.hh"
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

enum { T_ETH_SRC, T_ETH_DST, T_ETH_TYPE };

namespace IPSummaryDump {

static bool link_extract(PacketDesc& d, int thunk)
{
    const unsigned char *mac = d.p->mac_header();
    if (!mac && d.p->network_header() && d.p->data() < d.p->network_header())
	mac = d.p->data();
    const unsigned char *network = d.p->network_header();
    if (!network)
	network = d.p->end_data();
    switch (thunk & ~B_TYPEMASK) {

	// IP header properties
#define CHECK() do { if (!mac || mac + 14 != network) return field_missing(d, MISSING_ETHERNET, 0); } while (0)	
      case T_ETH_SRC:
	CHECK();
	d.vptr[0] = mac + 6;
	return true;
      case T_ETH_DST:
	CHECK();
	d.vptr[0] = mac;
	return true;
    case T_ETH_TYPE:
	CHECK();
	d.v = (mac[12]<<8) + mac[13];
	return true;
#undef CHECK

      default:
	return false;
    }
}

static void link_inject(PacketOdesc& d, int thunk)
{
    if (!d.p->mac_header() && !(d.p = d.p->push_mac_header(14)))
	return;
    switch (thunk) {
      case T_ETH_SRC:
	memcpy(d.p->ether_header()->ether_shost, d.u8, 6);
	break;
      case T_ETH_DST:
	memcpy(d.p->ether_header()->ether_dhost, d.u8, 6);
	break;
    case T_ETH_TYPE:
	d.p->ether_header()->ether_type = htons(d.v);
	if (d.v != ETHERTYPE_IP && d.v != ETHERTYPE_IP6)
	    d.is_ip = false;
	break;
    }
}

static void link_outa(const PacketDesc& d, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
      case T_ETH_SRC:
      case T_ETH_DST:
	*d.sa << EtherAddress(d.vptr[0]);
	break;
    case T_ETH_TYPE:
	d.sa->snprintf(4, "%04X", d.v);
	break;
    }
}

static bool link_ina(PacketOdesc& d, const String &str, int thunk)
{
    switch (thunk & ~B_TYPEMASK) {
    case T_ETH_SRC:
    case T_ETH_DST:
	return cp_ethernet_address(str, d.u8, d.e);
    case T_ETH_TYPE:
	return cp_integer(str, 16, &d.v) && d.v < 65536;
    default:
	return false;
    }
}

void link_register_unparsers()
{
    register_field("eth_src", T_ETH_SRC | B_6PTR, 0, order_link,
		   link_extract, link_inject, link_outa, link_ina, outb, inb);
    register_field("eth_dst", T_ETH_DST | B_6PTR, 0, order_link,
		   link_extract, link_inject, link_outa, link_ina, outb, inb);
    register_field("eth_type", T_ETH_DST | B_2, 0, order_link,
		   link_extract, link_inject, link_outa, link_ina, outb, inb);

    register_synonym("ether_src", "eth_src");
    register_synonym("ether_dst", "eth_dst");
    register_synonym("ether_type", "eth_type");
}

}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_Link)
CLICK_ENDDECLS
