// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_udp.{cc,hh} -- IP transport summary dump unparsers
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
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

#include "ipsumdump_udp.hh"
#include <click/packet.hh>
#include <click/nameinfo.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include <click/confparse.hh>
CLICK_DECLS

namespace IPSummaryDump {

enum { T_UDP_LEN };

static bool udp_extract(PacketDesc& d, const FieldWriter *f)
{
    int transport_length = d.transport_length();
    switch (f->user_data) {

#define CHECK(l) do { if (!d.udph || transport_length < (l)) return field_missing(d, IP_PROTO_UDP, (l)); } while (0)

      case T_UDP_LEN:
	CHECK(6);
	d.v = ntohs(d.udph->uh_ulen);
	return true;

#undef CHECK

      default:
	return false;
    }
}

static void udp_inject(PacketOdesc& d, const FieldReader *f)
{
    if (!d.make_ip(0) || !d.make_transp())
	return;
    int ip_p = d.p->ip_header()->ip_p;
    if (ip_p && ip_p != IP_PROTO_UDP && ip_p != IP_PROTO_UDPLITE)
	return;
    if (d.p->transport_length() < (int) sizeof(click_udp)
	&& !(d.p = d.p->put(sizeof(click_udp) - d.p->transport_length())))
	return;

    switch (f->user_data) {
    case T_UDP_LEN:
	d.p->udp_header()->uh_ulen = htons(d.v);
	break;
    }
}

static const FieldWriter udp_writers[] = {
    { "udp_len", B_4, T_UDP_LEN,
      ip_prepare, udp_extract, num_outa, outb }
};

static const FieldReader udp_readers[] = {
    { "udp_len", B_4, T_UDP_LEN, order_transp,
      num_ina, inb, udp_inject }
};

}

void IPSummaryDump_UDP::static_initialize()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(udp_writers) / sizeof(udp_writers[0]); ++i)
	FieldWriter::add(&udp_writers[i]);
    for (size_t i = 0; i < sizeof(udp_readers) / sizeof(udp_readers[0]); ++i)
	FieldReader::add(&udp_readers[i]);
}

void IPSummaryDump_UDP::static_cleanup()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(udp_writers) / sizeof(udp_writers[0]); ++i)
	FieldWriter::remove(&udp_writers[i]);
    for (size_t i = 0; i < sizeof(udp_readers) / sizeof(udp_readers[0]); ++i)
	FieldReader::remove(&udp_readers[i]);
}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_UDP)
CLICK_ENDDECLS
