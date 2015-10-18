/*
 * EtherRewrite.{cc,hh} -- encapsulates packet in Ethernet header
 *
 * Copyright (c) 2015 University of Liege
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
#include "etherrewrite.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

EtherRewrite::EtherRewrite()
{
}

EtherRewrite::~EtherRewrite()
{
}

int
EtherRewrite::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint16_t ether_type;
    if (Args(conf, this, errh)
	.read_mp("SRC", EtherAddressArg(), _ethh.ether_shost)
	.read_mp("DST", EtherAddressArg(), _ethh.ether_dhost)
	.complete() < 0)
	return -1;
    return 0;
}

inline Packet *
EtherRewrite::smaction(Packet *p)
{
    WritablePacket* q = p->uniqueify();
    if (q) {
        memcpy(q->mac_header(), &_ethh, 12);
    }
    return q;
}

void
EtherRewrite::push(int, Packet *p)
{
    output(0).push(smaction(p));
}

Packet *
EtherRewrite::pull(int)
{
    if (Packet *p = input(0).pull())
	return smaction(p);
    else
	return 0;
}

void
EtherRewrite::add_handlers()
{
    add_data_handlers("src", Handler::h_read, reinterpret_cast<EtherAddress *>(&_ethh.ether_shost));
    add_write_handler("src", reconfigure_keyword_handler, "1 SRC");
    add_data_handlers("dst", Handler::h_read, reinterpret_cast<EtherAddress *>(&_ethh.ether_dhost));
    add_write_handler("dst", reconfigure_keyword_handler, "2 DST");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EtherRewrite)
