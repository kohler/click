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

#include "ipsumdump_link.hh"
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/args.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

enum { T_ETH_SRC, T_ETH_DST, T_ETH_TYPE };

namespace IPSummaryDump {

static bool link_extract(PacketDesc& d, const FieldWriter *f)
{
    const unsigned char *mac;
    if (!d.p->has_mac_header()) {
	if (!d.p->has_network_header() || d.p->data() < d.p->network_header())
	    mac = d.p->data();
	else
	    mac = 0;
    } else
	mac = d.p->mac_header();

    const unsigned char *network;
    if (!d.p->has_network_header())
	network = d.p->end_data();
    else
	network = d.p->network_header();

    switch (f->user_data) {

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

static void link_inject(PacketOdesc& d, const FieldReader *f)
{
    if (!d.p->mac_header()) {
        if (!(d.p = d.p->push_mac_header(14)))
            return;
        d.p->ether_header()->ether_type = htons(ETHERTYPE_IP);
    }
    switch (f->user_data) {
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

static void link_outa(const PacketDesc& d, const FieldWriter *f)
{
    switch (f->user_data) {
      case T_ETH_SRC:
      case T_ETH_DST:
	*d.sa << EtherAddress(d.vptr[0]);
	break;
    case T_ETH_TYPE:
	d.sa->snprintf(4, "%04X", d.v);
	break;
    }
}

static bool link_ina(PacketOdesc& d, const String &str, const FieldReader *f)
{
    switch (f->user_data) {
    case T_ETH_SRC:
    case T_ETH_DST:
	return EtherAddressArg().parse(str, d.u8, d.e);
    case T_ETH_TYPE:
	return IntArg(16).parse(str, d.v) && d.v < 65536;
    default:
	return false;
    }
}

static const FieldWriter link_writers[] = {
    { "eth_src", B_6PTR, T_ETH_SRC,
      0, link_extract, link_outa, outb },
    { "eth_dst", B_6PTR, T_ETH_DST,
      0, link_extract, link_outa, outb },
    { "eth_type", B_2, T_ETH_TYPE,
      0, link_extract, link_outa, outb }
};

static const FieldReader link_readers[] = {
    { "eth_src", B_6PTR, T_ETH_SRC, order_link,
      link_ina, inb, link_inject },
    { "eth_dst", B_6PTR, T_ETH_DST, order_link,
      link_ina, inb, link_inject },
    { "eth_type", B_2, T_ETH_TYPE, order_link,
      link_ina, inb, link_inject }
};

static const FieldSynonym link_synonyms[] = {
    { "ether_src", "eth_src" },
    { "ether_dst", "eth_dst" },
    { "ether_type", "eth_type" }
};

}

void IPSummaryDump_Link::static_initialize()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(link_writers) / sizeof(link_writers[0]); ++i)
	FieldWriter::add(&link_writers[i]);
    for (size_t i = 0; i < sizeof(link_readers) / sizeof(link_readers[0]); ++i)
	FieldReader::add(&link_readers[i]);
    for (size_t i = 0; i < sizeof(link_synonyms) / sizeof(link_synonyms[0]); ++i)
	FieldSynonym::add(&link_synonyms[i]);
}

void IPSummaryDump_Link::static_cleanup()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(link_writers) / sizeof(link_writers[0]); ++i)
	FieldWriter::remove(&link_writers[i]);
    for (size_t i = 0; i < sizeof(link_readers) / sizeof(link_readers[0]); ++i)
	FieldReader::remove(&link_readers[i]);
    for (size_t i = 0; i < sizeof(link_synonyms) / sizeof(link_synonyms[0]); ++i)
	FieldSynonym::remove(&link_synonyms[i]);
}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_Link)
CLICK_ENDDECLS
