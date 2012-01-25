/*
 * aggregateip.{cc,hh} -- set aggregate annotation based on IP field
 * Eddie Kohler
 *
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2005-2006 Regents of the University of California
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
#include "aggregateip.hh"
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/packet_anno.hh>
CLICK_DECLS

AggregateIP::AggregateIP()
{
}

AggregateIP::~AggregateIP()
{
}

int
AggregateIP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String arg;
    _incremental = _unshift_ip_addr = false;
    if (Args(conf, this, errh)
	.read_mp("FIELD", AnyArg(), arg)
	.read("INCREMENTAL", _incremental)
	.read("UNSHIFT_IP_ADDR", _unshift_ip_addr)
	.complete() < 0)
	return -1;
    const char *end = IPField::parse(arg.begin(), arg.end(), -1, &_f, errh, this);
    if (end == arg.begin())
	return -1;
    else if (end != arg.end())
	return errh->error("garbage after field specification");
    int32_t right = _f.bit_offset() + _f.bit_length() - 1;
    if ((_f.bit_offset() / 32) != (right / 32))
	return errh->error("field specification does not fit within a single word");
    if (_f.bit_length() > 32)
	return errh->error("field length too large, max 32");
    else if (_f.bit_length() == 32 && _incremental)
	return errh->error("'INCREMENTAL' is incompatible with field length 32");
    if (_f.bit_length() == 32)
	_mask = 0xFFFFFFFFU;
    else
	_mask = (1 << _f.bit_length()) - 1;
    _offset = (_f.bit_offset() / 32) * 4;
    if (!_unshift_ip_addr || _f.proto() != 0 || _f.bit_offset() < 12*8 || _f.bit_offset() >= 20*8)
	_shift = 31 - right % 32;
    else {
	_mask <<= 31 - right % 32;
	_shift = 0;
    }
    return 0;
}

Packet *
AggregateIP::bad_packet(Packet *p)
{
    if (noutputs() == 2)
	output(1).push(p);
    else
	p->kill();
    return 0;
}

Packet *
AggregateIP::handle_packet(Packet *p)
{
    if (!p->has_network_header())
	return bad_packet(p);

    const click_ip *iph = p->ip_header();
    int offset = p->length();
    switch (_f.proto()) {
      case 0:
	offset = p->network_header_offset();
	break;
      case IP_PROTO_TCP_OR_UDP:
	if (IP_FIRSTFRAG(iph) && (iph->ip_p == IP_PROTO_TCP || iph->ip_p == IP_PROTO_UDP))
	    offset = p->transport_header_offset();
	break;
      case IP_PROTO_TRANSP:
	if (IP_FIRSTFRAG(iph))
	    offset = p->transport_header_offset();
	break;
      case IP_PROTO_PAYLOAD:
	if (!IP_FIRSTFRAG(iph))
	    /* bad; will be thrown away below */;
	else if (iph->ip_p == IPPROTO_TCP && p->transport_header_offset() + sizeof(click_tcp) <= p->length()) {
	    const click_tcp *tcph = (const click_tcp *)p->transport_header();
	    offset = p->transport_header_offset() + (tcph->th_off << 2);
	} else if (iph->ip_p == IPPROTO_UDP)
	    offset = p->transport_header_offset() + sizeof(click_udp);
	break;
      default:
	if (IP_FIRSTFRAG(iph) && iph->ip_p == _f.proto())
	    offset = p->transport_header_offset();
	break;
    }
    offset += _offset;

    if (offset + 4 > (int)p->length())
	return bad_packet(p);

    uint32_t udata = *((const uint32_t *)(p->data() + offset));
    uint32_t agg = (ntohl(udata) >> _shift) & _mask;

    if (_incremental)
	SET_AGGREGATE_ANNO(p, (AGGREGATE_ANNO(p) << _f.bit_length()) + agg);
    else
	SET_AGGREGATE_ANNO(p, agg);

    return p;
}

void
AggregateIP::push(int, Packet *p)
{
    if (Packet *q = handle_packet(p))
	output(0).push(q);
}

Packet *
AggregateIP::pull(int)
{
    Packet *p = input(0).pull();
    if (p)
	p = handle_packet(p);
    return p;
}

String
AggregateIP::read_handler(Element *e, void *thunk)
{
    AggregateIP *aip = static_cast<AggregateIP *>(e);
    switch ((intptr_t)thunk) {
      case 0:
	return NameInfo::revquery_int(NameInfo::T_IP_PROTO, e, aip->_f.proto());
      case 1:
	return String(aip->_f.bit_offset());
      case 2:
	return String(aip->_f.bit_length());
      case 3:
	return aip->_f.unparse(e, false);
      default:
	return "<error>";
    }
}

void
AggregateIP::add_handlers()
{
    add_read_handler("header", read_handler, 0);
    add_read_handler("bit_offset", read_handler, 1);
    add_read_handler("bit_length", read_handler, 2);
    add_read_handler("field", read_handler, 3);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel IPFieldInfo)
EXPORT_ELEMENT(AggregateIP)
