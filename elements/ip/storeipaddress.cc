/*
 * storeipaddress.{cc,hh} -- element stores IP destination annotation into
 * packet
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2007 Regents of the University of California
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
#include "storeipaddress.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
CLICK_DECLS

StoreIPAddress::StoreIPAddress()
{
}

StoreIPAddress::~StoreIPAddress()
{
}

int
StoreIPAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String offset;
    int r;
    _use_address = conf.size() > 1;
    if (!_use_address)
	r = Args(conf, this, errh).read_mp("OFFSET", WordArg(), offset).complete();
    else
	r = Args(conf, this, errh).read_mp("ADDR", _address)
	    .read_mp("OFFSET", WordArg(), offset).complete();
    if (r < 0)
	return r;
    if (offset.lower() == "src")
	_offset = -12;
    else if (offset.lower() == "dst")
	_offset = -16;
    else if (!IntArg().parse(offset, _offset) || _offset < 0)
	return errh->error("type mismatch: OFFSET requires integer");
    return 0;
}

Packet *
StoreIPAddress::simple_action(Packet *p)
{
    // XXX error reporting?
    IPAddress ipa = (_use_address ? _address : p->dst_ip_anno());
    if ((ipa || _use_address) && (uint32_t) _offset + 4 <= p->length()) {
	if (WritablePacket *q = p->uniqueify()) {
	    memcpy(q->data() + _offset, &ipa, 4);
	    return q;
	} else
	    return 0;

    } else if (_offset >= -16 && p->has_network_header()
	       && p->ip_header_length() >= sizeof(click_ip)) {
	// special case: store IP address into IP header
	// and update checksums incrementally
	if (WritablePacket *q = p->uniqueify()) {
	    uint16_t *x = reinterpret_cast<uint16_t *>(q->network_header() - _offset);
	    uint32_t old_hw = (uint32_t) x[0] + x[1];
	    old_hw += (old_hw >> 16);

	    memcpy(x, &ipa, 4);

	    uint32_t new_hw = (uint32_t) x[0] + x[1];
	    new_hw += (new_hw >> 16);
	    click_ip *iph = q->ip_header();
	    click_update_in_cksum(&iph->ip_sum, old_hw, new_hw);
	    if (iph->ip_p == IP_PROTO_TCP && IP_FIRSTFRAG(iph)
		&& q->transport_length() >= (int) sizeof(click_tcp))
		click_update_in_cksum(&q->tcp_header()->th_sum, old_hw, new_hw);
	    if (iph->ip_p == IP_PROTO_UDP && IP_FIRSTFRAG(iph)
		&& q->transport_length() >= (int) sizeof(click_udp)
		&& q->udp_header()->uh_sum)
		click_update_in_cksum(&q->udp_header()->uh_sum, old_hw, new_hw);

	    return q;
	} else
	    return 0;

    } else {
	checked_output_push(1, p);
	return 0;
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StoreIPAddress)
ELEMENT_MT_SAFE(StoreIPAddress)
