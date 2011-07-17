/*
 * unstripdsrheader.{cc,hh} -- put IP header back based on annotation
 *
 * Based on work by Eddie Kohler, Shweta Bhandare, Sagar Sanghani,
 * Sheetalkumar Doshi, Timothy X Brown Daniel Aguayo, Benjie Chen
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 * Copyright (c) 2003 University of Colorado at Boulder
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
#include "unstripdsrheader.hh"
#include <click/packet_anno.hh>
#include <clicknet/ip.h>
#include "dsr.hh"
CLICK_DECLS

UnstripDSRHeader::UnstripDSRHeader()
{
}

UnstripDSRHeader::~UnstripDSRHeader()
{
}

Packet *
UnstripDSRHeader::simple_action(Packet *p_in)
{
    if (VLAN_TCI_ANNO(p_in) == 0) {
	// click_chatter("UnstripDSR: No VLAN_ANNO, no DSR unstripping");
	return p_in;
    }
    if (VLAN_TCI_ANNO(p_in) == 1) {
	// click_chatter("UnstripDSR: Packet had been marked non-payload");
	SET_VLAN_TCI_ANNO(p_in, 0);
	return p_in;
    }

    ptrdiff_t dsr_len = ntohs(VLAN_TCI_ANNO(p_in));

    WritablePacket *p = p_in->uniqueify();
    click_dsr *dsr_old = reinterpret_cast<click_dsr *>(p->data() - dsr_len);
    click_dsr *dsr_new = reinterpret_cast<click_dsr *>(p->data() - dsr_len +
							sizeof(click_ip));
    click_ip *ip = (click_ip *)(p->data());

    // save the IP header
    click_ip new_ip;
    memcpy(&new_ip, ip, sizeof(click_ip));
    new_ip.ip_p = IP_PROTO_DSR;

    // fetch the original DSR part from what once was the ip header (swap them)
    memcpy(dsr_new, dsr_old, dsr_len);

    // un-remove the headers
    p = p->push(dsr_len);	// should never create a new packet
    // clear VLAN_TCI_ANNO
    SET_VLAN_TCI_ANNO(p, 0);

    memcpy(p->data(), &new_ip, sizeof(click_ip));
    ip=reinterpret_cast<click_ip *>(p->data());
    ip->ip_len=htons(p->length());
    ip->ip_sum=0;
    ip->ip_sum=click_in_cksum((unsigned char *)ip,sizeof(click_ip));

    p->set_ip_header((click_ip*)p->data(),sizeof(click_ip));

    // click_chatter("UnstripDSR: Added %d bytes\n", dsr_len);

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(UnstripDSRHeader)
ELEMENT_MT_SAFE(UnstripDSRHeader)
