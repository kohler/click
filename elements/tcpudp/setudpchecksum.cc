// -*- c-basic-offset: 4 -*-
/*
 * setudpchecksum.{cc,hh} -- sets the UDP header checksum
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
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
#include "setudpchecksum.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
CLICK_DECLS

SetUDPChecksum::SetUDPChecksum()
{
}

SetUDPChecksum::~SetUDPChecksum()
{
}

Packet *
SetUDPChecksum::simple_action(Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    if (!p)
	return 0;

    // XXX check IP header/UDP protocol?
    click_ip *iph = p->ip_header();
    click_udp *udph = p->udp_header();
    int len;
    if (IP_ISFRAG(iph)
	|| p->transport_length() < (int) sizeof(click_udp)
	|| (len = ntohs(udph->uh_ulen),
	    p->transport_length() < len)) {
	// fragment, or packet data too short
	if (noutputs() == 1) {
	    void *&x = router()->force_attachment("SetUDPChecksum_message");
	    if (!x) {
		click_chatter("%p{element}: fragment or short packet", this);
		x = this;
	    }
	}
	checked_output_push(1, p);
	return 0;
    }

    udph->uh_sum = 0;
    unsigned csum = click_in_cksum((unsigned char *)udph, len);
    udph->uh_sum = click_in_cksum_pseudohdr(csum, iph, len);

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetUDPChecksum)
ELEMENT_MT_SAFE(SetUDPChecksum)
