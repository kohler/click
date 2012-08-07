/*
 * etherencap.{cc,hh} -- encapsulates packet in Ethernet header
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "etherencap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

EtherEncap::EtherEncap()
{
}

EtherEncap::~EtherEncap()
{
}

int
EtherEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint16_t ether_type;
    click_ether ethh;
    if (Args(conf, this, errh)
	.read_mp("ETHERTYPE", ether_type)
	.read_mp("SRC", EtherAddressArg(), ethh.ether_shost)
	.read_mp("DST", EtherAddressArg(), ethh.ether_dhost)
	.complete() < 0)
	return -1;
    ethh.ether_type = htons(ether_type);
    _ethh = ethh;
    return 0;
}

Packet *
EtherEncap::smaction(Packet *p)
{
    if (WritablePacket *q = p->push_mac_header(14)) {
	memcpy(q->data(), &_ethh, 14);
	return q;
    } else
	return 0;
}

void
EtherEncap::push(int, Packet *p)
{
    if (Packet *q = smaction(p))
	output(0).push(q);
}

Packet *
EtherEncap::pull(int)
{
    if (Packet *p = input(0).pull())
	return smaction(p);
    else
	return 0;
}

void
EtherEncap::add_handlers()
{
    add_data_handlers("src", Handler::h_read, reinterpret_cast<EtherAddress *>(&_ethh.ether_shost));
    add_write_handler("src", reconfigure_keyword_handler, "1 SRC");
    add_data_handlers("dst", Handler::h_read, reinterpret_cast<EtherAddress *>(&_ethh.ether_dhost));
    add_write_handler("dst", reconfigure_keyword_handler, "2 DST");
    add_net_order_data_handlers("ethertype", Handler::h_read, &_ethh.ether_type);
    add_write_handler("ethertype", reconfigure_keyword_handler, "0 ETHERTYPE");
    add_net_order_data_handlers("etht", Handler::h_read | Handler::h_deprecated, &_ethh.ether_type);
    add_write_handler("etht", reconfigure_keyword_handler, "0 ETHERTYPE");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EtherEncap)
