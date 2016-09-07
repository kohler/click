/*
 * fragmentationencap.{cc,hh} -- encapsulates packet in a Fragmentation Extension header
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
#include "fragmentationencap.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip6.h>
CLICK_DECLS

FragmentationEncap::FragmentationEncap()
{
}

FragmentationEncap::~FragmentationEncap()
{
}

int
FragmentationEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{   
    _identification = 0;
    bool more_fragments;

    if (Args(conf, this, errh)
	.read_mp("PROTO", IntArg(), _next_header)
	.read_mp("OFFSET", IntArg(), _offset)
	.read_mp("M", BoolArg(), more_fragments)
	.read_p("ID", IntArg(), _identification)
	.complete() < 0)
	    return -1;
	    
    if (_offset > 8191) {
        return errh->error("OFFSET should be an integer between 0 and 8191");
    }
        
	    
    _offset = htons((_offset << 3) | more_fragments);       // Explanation:
                                                            // The ip6_fragment struct has the field ip6_frag_offset which contains
                                                            // a) the actual offset on bits 0-13
                                                            // b) 2 reserved bits that are zero on bits 14-15
                                                            // c) the M field which is a bit that tells whether more fragments follow or not
                                                            // 
                                                            // the actual variable will be in the first 13 bits (and must be shifted with 3 to the left)
                                                            // & at the same time we get 0 on the last 3 bits for which the bits 14-15 needed to be zero anyway
                                                            //
                                                            // since the last bit is already zero we just or him with a bit that tells whether or not we got more fragments or not
	    
	_identification = htonl(_identification);    

    return 0;
}

Packet *
FragmentationEncap::simple_action(Packet *p_in)
{
    WritablePacket *p = p_in->push(sizeof(click_ip6_fragment)); // make room for the new Fragmentation extension header

    if (!p)
        return 0;

    click_ip6_fragment *fragmentation_header = reinterpret_cast<click_ip6_fragment *>(p->data());

    // set the values of the fragmentation extension header
    fragmentation_header->ip6_frag_nxt = _next_header;
    fragmentation_header->ip6_frag_reserved = 0;
    fragmentation_header->ip6_frag_offset = _offset;
    fragmentation_header->ip6_frag_id = _identification;
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FragmentationEncap)
