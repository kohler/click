/*
 * hopbyhopencap.{cc,hh} -- encapsulates packet in a Pad6 Hop-by-Hop extension header
 * Glenn Minne
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
#include "hopbyhopencap.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip6.h>
CLICK_DECLS

HopByHopEncap::HopByHopEncap()
{
}

HopByHopEncap::~HopByHopEncap()
{
}

int
HopByHopEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	.read_mp("PROTO", IntArg(), _next_header)
	.complete() < 0)
	    return -1;    
    
    return 0;
}

Packet *
HopByHopEncap::simple_action(Packet *p_in)
{
    // A Hop-by-Hop header has a common part followed by an option part, which contains one or multiple option.
    // This header contains one option, the Pad6 option, a special type of a PadN option. It adds 6 bytes of padding to 
    // our Hop-by-hop Extension Header.
    struct HopByHopOptionsWithPad6 {
        // common hop-byhop extension header part
        uint8_t ip6h_nxt;           /* next header */
        uint8_t ip6h_len;           /* 8-bit unsigned integer.  Length of the
                                       Destination Options header in 8-octet units, not
                                       including the first 8 octets. */
        // the actual Pad6 option
        uint8_t option_type;        // Every PadN option including Pad6 has an option type of 1.
        uint8_t option_data_len;    // This explains about which specific PadN option we are talking about. We are talking about Pad6 so we need to insert 4.
        uint32_t option_data;       // Here are our 4 remaining bits of data so that we have in total 6 bytes of padding. These bits must all be set to zero.
                                    // put all zeroes here.
    };

    WritablePacket *p = p_in->push(8); // make room for the new Destination Options extension header

    if (!p)
        return 0;

    HopByHopOptionsWithPad6 *hop_by_hop_header = reinterpret_cast<HopByHopOptionsWithPad6 *>(p->data());

    // set the values of the Hop-by-Hop extension header
    hop_by_hop_header->ip6h_nxt = _next_header;
    hop_by_hop_header->ip6h_len = 0;
    
    // set the values of the Pad6 option
    hop_by_hop_header->option_type = 1;
    hop_by_hop_header->option_data_len = 4;
    hop_by_hop_header->option_data = 0;

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(HopByHopEncap)
