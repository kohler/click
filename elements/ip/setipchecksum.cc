/*
 * setipchecksum.{cc,hh} -- element sets IP header checksum
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2012 Eddie Kohler
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
#include "setipchecksum.hh"
#include <click/glue.hh>
#include <clicknet/ip.h>
CLICK_DECLS

SetIPChecksum::SetIPChecksum()
    : _drops(0)
{
}

SetIPChecksum::~SetIPChecksum()
{
}

Packet *
SetIPChecksum::simple_action(Packet *p_in)
{
    if (WritablePacket *p = p_in->uniqueify()) {
	unsigned char *nh_data = (p->has_network_header() ? p->network_header() : p->data());
	click_ip *iph = reinterpret_cast<click_ip *>(nh_data);
	unsigned plen = p->end_data() - nh_data, hlen;

	if (likely(plen >= sizeof(click_ip))
	    && likely((hlen = iph->ip_hl << 2) >= sizeof(click_ip))
	    && likely(hlen <= plen)) {
	    iph->ip_sum = 0;
	    iph->ip_sum = click_in_cksum((unsigned char *) iph, hlen);
	    return p;
	}

	if (++_drops == 1)
	    click_chatter("SetIPChecksum: bad input packet");
	p->kill();
    }
    return 0;
}

void
SetIPChecksum::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetIPChecksum)
ELEMENT_MT_SAFE(SetIPChecksum)
