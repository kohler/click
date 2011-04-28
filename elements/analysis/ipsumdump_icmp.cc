// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_icmp.{cc,hh} -- IP transport summary dump unparsers
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

#include "ipsumdump_icmp.hh"
#include <click/packet.hh>
#include <click/nameinfo.hh>
#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <click/confparse.hh>
CLICK_DECLS

namespace IPSummaryDump {

enum { T_ICMP_TYPE, T_ICMP_TYPE_NAME, T_ICMP_CODE, T_ICMP_CODE_NAME,
       T_ICMP_FLOWID, T_ICMP_SEQ, T_ICMP_NEXTMTU };

enum {
    ICMP_TYPE_HAVE_FLOW = ((1U << ICMP_ECHO) | (1U << ICMP_ECHOREPLY)
			   | (1U << ICMP_IREQ) | (1U << ICMP_IREQREPLY)
			   | (1U << ICMP_TSTAMP) | (1U << ICMP_TSTAMPREPLY))
};

static bool icmp_extract(PacketDesc& d, const FieldWriter *f)
{
    int transport_length = d.transport_length();
    switch (f->user_data) {

#define CHECK(l) do { if (!d.icmph || transport_length < (l)) return field_missing(d, IP_PROTO_ICMP, (l)); } while (0)

      case T_ICMP_TYPE:
      case T_ICMP_TYPE_NAME:
	CHECK(1);
	d.v = d.icmph->icmp_type;
	return true;

      case T_ICMP_CODE:
      case T_ICMP_CODE_NAME:
	CHECK(2);
	d.v = d.icmph->icmp_code;
	return true;

      case T_ICMP_FLOWID:
	CHECK(1);
	if (d.icmph->icmp_type >= 32
	    || (ICMP_TYPE_HAVE_FLOW & (1 << d.icmph->icmp_type)) == 0)
	    return false;
	CHECK(6);
	d.v = ntohs(reinterpret_cast<const click_icmp_sequenced *>(d.icmph)->icmp_identifier);
	return true;

      case T_ICMP_SEQ:
	CHECK(1);
	if (d.icmph->icmp_type >= 32
	    || (ICMP_TYPE_HAVE_FLOW & (1 << d.icmph->icmp_type)) == 0)
	    return false;
	CHECK(8);
	d.v = ntohs(reinterpret_cast<const click_icmp_sequenced *>(d.icmph)->icmp_sequence);
	return true;

      case T_ICMP_NEXTMTU:
	CHECK(2);
	if (d.icmph->icmp_type != ICMP_UNREACH
	    || d.icmph->icmp_code != ICMP_UNREACH_NEEDFRAG)
	    return false;
	CHECK(8);
	d.v = ntohs(reinterpret_cast<const click_icmp_needfrag *>(d.icmph)->icmp_nextmtu);
	return true;

#undef CHECK

      default:
	return false;
    }
}

static void icmp_inject(PacketOdesc& d, const FieldReader *f)
{
    if (!d.make_ip(IP_PROTO_ICMP) || !d.make_transp())
	return;
    if (d.p->transport_length() < (int) sizeof(click_icmp)
	&& !(d.p = d.p->put(sizeof(click_icmp) - d.p->transport_length())))
	return;

    click_icmp *icmph = d.p->icmp_header();
    switch (f->user_data) {
    case T_ICMP_TYPE:
    case T_ICMP_TYPE_NAME: {
	icmph->icmp_type = d.v;
	d.have_icmp_type = true;
	int len = click_icmp_hl(d.v);
	if (d.p->transport_length() < len
	    && !(d.p = d.p->put(len - d.p->transport_length())))
	    return;
	break;
    }
    case T_ICMP_CODE:
    case T_ICMP_CODE_NAME:
	icmph->icmp_code = d.v;
	d.have_icmp_code = true;
	break;
    case T_ICMP_FLOWID:
	if (!d.have_icmp_type
	    || (icmph->icmp_type < 32
		&& (ICMP_TYPE_HAVE_FLOW & (1 << icmph->icmp_type)))) {
	    if (d.p->transport_length() < (int) sizeof(click_icmp_sequenced)) {
		if (!(d.p = d.p->put(sizeof(click_icmp_sequenced) - d.p->transport_length())))
		    return;
		icmph = d.p->icmp_header();
	    }
	    reinterpret_cast<click_icmp_sequenced *>(icmph)->icmp_identifier = htons(d.v);
	}
	break;
    case T_ICMP_SEQ:
	if (!d.have_icmp_type
	    || (icmph->icmp_type < 32
		&& (ICMP_TYPE_HAVE_FLOW & (1 << icmph->icmp_type)))) {
	    if (d.p->transport_length() < (int) sizeof(click_icmp_sequenced)) {
		if (!(d.p = d.p->put(sizeof(click_icmp_sequenced) - d.p->transport_length())))
		    return;
		icmph = d.p->icmp_header();
	    }
	    reinterpret_cast<click_icmp_sequenced *>(icmph)->icmp_sequence = htons(d.v);
	}
	break;
    case T_ICMP_NEXTMTU:
	if ((!d.have_icmp_type || icmph->icmp_type == ICMP_UNREACH)
	    && (!d.have_icmp_code || icmph->icmp_code == ICMP_UNREACH_NEEDFRAG)) {
	    if (d.p->transport_length() < (int) sizeof(click_icmp_needfrag)) {
		if (!(d.p = d.p->put(sizeof(click_icmp_needfrag) - d.p->transport_length())))
		    return;
		icmph = d.p->icmp_header();
	    }
	    reinterpret_cast<click_icmp_needfrag *>(icmph)->icmp_nextmtu = htons(d.v);
	}
	break;
    }
}

static void icmp_outa(const PacketDesc &d, const FieldWriter *f)
{
    switch (f->user_data) {
      case T_ICMP_TYPE_NAME:
	if (String s = NameInfo::revquery_int(NameInfo::T_ICMP_TYPE, d.e, d.v))
	    *d.sa << s;
	else
	    *d.sa << d.v;
	break;
      case T_ICMP_CODE_NAME:
	if (String s = NameInfo::revquery_int(NameInfo::T_ICMP_CODE + d.icmph->icmp_type, d.e, d.v))
	    *d.sa << s;
	else
	    *d.sa << d.v;
	break;
    }
}

static bool icmp_ina(PacketOdesc &d, const String &str, const FieldReader *f)
{
    switch (f->user_data) {
    case T_ICMP_TYPE:
    case T_ICMP_TYPE_NAME:
	if (NameInfo::query_int(NameInfo::T_ICMP_TYPE, d.e, str, &d.v)
	    && d.v < 256)
	    return true;
	break;
    case T_ICMP_CODE:
    case T_ICMP_CODE_NAME:
	if (d.have_icmp_type) {
	    if (NameInfo::query_int(NameInfo::T_ICMP_CODE + d.p->icmp_header()->icmp_type, d.e, str, &d.v)
		&& d.v < 256)
		return true;
	} else {
	    if (IntArg().parse(str, d.v) && d.v < 256)
		return true;
	}
	break;
    }
    return false;
}

static const FieldWriter icmp_writers[] = {
    { "icmp_type", B_1, T_ICMP_TYPE,
      ip_prepare, icmp_extract, num_outa, outb },
    { "icmp_code", B_1, T_ICMP_CODE,
      ip_prepare, icmp_extract, num_outa, outb },
    { "icmp_type_name", B_1, T_ICMP_TYPE_NAME,
      ip_prepare, icmp_extract, icmp_outa, outb },
    { "icmp_code_name", B_1, T_ICMP_CODE_NAME,
      ip_prepare, icmp_extract, icmp_outa, outb },
    { "icmp_flowid", B_2, T_ICMP_FLOWID,
      ip_prepare, icmp_extract, num_outa, outb },
    { "icmp_seq", B_2, T_ICMP_SEQ,
      ip_prepare, icmp_extract, num_outa, outb },
    { "icmp_nextmtu", B_2, T_ICMP_NEXTMTU,
      ip_prepare, icmp_extract, num_outa, outb }
};

static const FieldReader icmp_readers[] = {
    { "icmp_type", B_1, T_ICMP_TYPE, order_transp,
      icmp_ina, inb, icmp_inject },
    { "icmp_code", B_1, T_ICMP_CODE, order_transp + 1,
      icmp_ina, inb, icmp_inject },
    { "icmp_type_name", B_1, T_ICMP_TYPE_NAME, order_transp,
      icmp_ina, inb, icmp_inject },
    { "icmp_code_name", B_1, T_ICMP_CODE_NAME, order_transp + 1,
      icmp_ina, inb, icmp_inject },
    { "icmp_flowid", B_2, T_ICMP_FLOWID, order_transp + 2,
      num_ina, inb, icmp_inject },
    { "icmp_seq", B_2, T_ICMP_SEQ, order_transp + 2,
      num_ina, inb, icmp_inject },
    { "icmp_nextmtu", B_2, T_ICMP_NEXTMTU, order_transp + 2,
      num_ina, inb, icmp_inject }
};

}

void IPSummaryDump_ICMP::static_initialize()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(icmp_writers) / sizeof(icmp_writers[0]); ++i)
	FieldWriter::add(&icmp_writers[i]);
    for (size_t i = 0; i < sizeof(icmp_readers) / sizeof(icmp_readers[0]); ++i)
	FieldReader::add(&icmp_readers[i]);
}

void IPSummaryDump_ICMP::static_cleanup()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(icmp_writers) / sizeof(icmp_writers[0]); ++i)
	FieldWriter::remove(&icmp_writers[i]);
    for (size_t i = 0; i < sizeof(icmp_readers) / sizeof(icmp_readers[0]); ++i)
	FieldReader::remove(&icmp_readers[i]);
}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_ICMP)
CLICK_ENDDECLS
