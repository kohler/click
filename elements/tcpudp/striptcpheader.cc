// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * striptcpheader.{cc,hh} -- element strips TCP header from front of packet
 * Tom Barbette
 *
 * Copyright (c) 2020 KTH Royal Institute of Technology
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/tcp.h>
#include "striptcpheader.hh"
CLICK_DECLS

StripTCPHeader::StripTCPHeader()
{
}

Packet *
StripTCPHeader::simple_action(Packet *p)
{
	const click_tcp *th = p->tcp_header();
	unsigned n = th->th_off << 2;
	p->pull(n);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StripTCPHeader)
ELEMENT_MT_SAFE(StripTCPHeader)
