/*
 * setipdscp.{cc,hh} -- element sets IP header DSCP field
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "setipdscp.hh"
#include <clicknet/ip.h>
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

SetIPDSCP::SetIPDSCP()
{
}

SetIPDSCP::~SetIPDSCP()
{
}

int
SetIPDSCP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned dscp_val;
    if (Args(conf, this, errh).read_mp("DSCP", dscp_val).complete() < 0)
	return -1;
    if (dscp_val > 0x3F)
	return errh->error("diffserv code point out of range");
    // OK: set values
    _dscp = (dscp_val << 2);
    return 0;
}

inline Packet *
SetIPDSCP::smaction(Packet *p)
{
    assert(p->has_network_header());
    WritablePacket *q;
    if (!(q = p->uniqueify()))
	return 0;
    click_ip *ip = q->ip_header();
    uint16_t old_hw = (reinterpret_cast<uint16_t *>(ip))[0];
    ip->ip_tos = (ip->ip_tos & 0x3) | _dscp;
    click_update_in_cksum(&ip->ip_sum, old_hw, reinterpret_cast<uint16_t *>(ip)[0]);
    return q;
}

void
SetIPDSCP::push(int, Packet *p)
{
    if ((p = smaction(p)) != 0)
	output(0).push(p);
}

Packet *
SetIPDSCP::pull(int)
{
    Packet *p = input(0).pull();
    if (p)
	p = smaction(p);
    return p;
}

void
SetIPDSCP::add_handlers()
{
    add_read_handler("dscp", read_keyword_handler, "0 DSCP", Handler::CALM);
    add_write_handler("dscp", reconfigure_keyword_handler, "0 DSCP");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetIPDSCP)
ELEMENT_MT_SAFE(SetIPDSCP)
