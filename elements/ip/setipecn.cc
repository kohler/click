/*
 * setipecn.{cc,hh} -- element sets IP header ECN field
 * Eddie Kohler
 *
 * Copyright (c) 2009 Intel Corporation
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
#include "setipecn.hh"
#include <clicknet/ip.h>
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

SetIPECN::SetIPECN()
{
}

SetIPECN::~SetIPECN()
{
}

int
SetIPECN::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String ecn;
    if (Args(conf, this, errh).read_mp("ECN", AnyArg(), ecn).complete() < 0)
	return -1;
    if (ecn.length() == 1 && ecn[0] >= '0' && ecn[0] <= '3')
	_ecn = ecn[0] - '0';
    else if (ecn.equals("no", 2) || ecn.equals("-", 1))
	_ecn = IP_ECN_NOT_ECT;
    else if (ecn.equals("ect1", 4) || ecn.equals("ECT(1)", 6))
	_ecn = IP_ECN_ECT1;
    else if (ecn.equals("ect2", 4) || ecn.equals("ECT(0)", 6))
	_ecn = IP_ECN_ECT2;
    else if (ecn.equals("ce", 2) || ecn.equals("CE", 2))
	_ecn = IP_ECN_CE;
    else
	return errh->error("bad ECN argument");
    return 0;
}

Packet *
SetIPECN::simple_action(Packet *p)
{
    assert(p->has_network_header());
    WritablePacket *q;
    if (!(q = p->uniqueify()))
	return 0;
    click_ip *ip = q->ip_header();
    uint16_t old_hw = (reinterpret_cast<uint16_t *>(ip))[0];
    ip->ip_tos = (ip->ip_tos & IP_DSCPMASK) | _ecn;
    click_update_in_cksum(&ip->ip_sum, old_hw, reinterpret_cast<uint16_t *>(ip)[0]);
    return q;
}

void
SetIPECN::add_handlers()
{
    add_read_handler("ecn", read_keyword_handler, "0 ECN", Handler::CALM);
    add_write_handler("ecn", reconfigure_keyword_handler, "0 ECN");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetIPECN)
ELEMENT_MT_SAFE(SetIPECN)
