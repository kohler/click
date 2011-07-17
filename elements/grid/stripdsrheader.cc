/*
 * stripdsrheader.{cc,hh} -- element removes DSR header (saving it for later)
 * Florian Sesser, Technical University of Munich, 2010
 *
 * Based on work by Eddie Kohler, Shweta Bhandare, Sagar Sanghani,
 * Sheetalkumar Doshi, Timothy X Brown Daniel Aguayo
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
#include "stripdsrheader.hh"
#include <click/packet_anno.hh>
#include <clicknet/ip.h>
#include "dsr.hh"
CLICK_DECLS

StripDSRHeader::StripDSRHeader()
{
}

StripDSRHeader::~StripDSRHeader()
{
}

Packet *
StripDSRHeader::simple_action(Packet *p)
{
    const click_dsr_option *dsr_option = (const click_dsr_option *)
					    (p->data() + sizeof(click_ip)
					     + sizeof(click_dsr));

    if (dsr_option->dsr_type != DSR_TYPE_SOURCE_ROUTE) {
	// this is not a DSR DATA packet -- do "nothing"
	// (but mark the packet to not unstrip it later)
	SET_VLAN_TCI_ANNO(p, 1);
	// click_chatter("StripDSR: Marked non-payload");
	return p;
    }

    return swap_headers(p);
}

// from dsrroutetable.cc, was called strip_headers()
// removes all DSR headers, leaving an ordinary IP packet
// changes: saves DSR header to later be able to undo the stripping
Packet *
StripDSRHeader::swap_headers(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  click_dsr *dsr = (click_dsr *)(p->data() + sizeof(click_ip));

  assert(ip->ip_p == IP_PROTO_DSR);

  // get the length of the DSR headers from the fixed header
  unsigned dsr_len = sizeof(click_dsr) + ntohs(dsr->dsr_len);

  // save the IP header
  click_ip new_ip;
  memcpy(&new_ip, ip, sizeof(click_ip));
  new_ip.ip_p = dsr->dsr_next_header;

  // save the to-be-overwritten DSR part to the orig. ip header (swap them)
  memcpy(ip, dsr, dsr_len);
  // save the offset to the VLAN_ANNO tag
  SET_VLAN_TCI_ANNO(p, htons(dsr_len));

  // remove the headers
  p->pull(dsr_len);

  memcpy(p->data(), &new_ip, sizeof(click_ip));
  ip=reinterpret_cast<click_ip *>(p->data());
  ip->ip_len=htons(p->length());
  ip->ip_sum=0;
  ip->ip_sum=click_in_cksum((unsigned char *)ip,sizeof(click_ip));

  p->set_ip_header((click_ip*)p->data(),sizeof(click_ip));

  // click_chatter("StripDSR: Removed %d bytes\n", dsr_len);

  return p;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(StripDSRHeader)
ELEMENT_MT_SAFE(StripDSRHeader)
