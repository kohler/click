/*
 * listenetherswitch.{cc,hh} -- learning, forwarding Ethernet bridge with listen port
 * Bart Braem
 * Based on the etherswitch by John Jannotti
 *
 * Copyright (c) 2005 University of Antwerp
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
#include "listenetherswitch.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/glue.hh>
CLICK_DECLS

ListenEtherSwitch::ListenEtherSwitch()
{
}

ListenEtherSwitch::~ListenEtherSwitch()
{
}

void
ListenEtherSwitch::push(int source, Packet *p)
{
    const click_ether* e = (const click_ether*) p->data();
    int outport = -1;		// Broadcast

    // 0 timeout means dumb switch
    if (_timeout != 0) {
	_table.set(EtherAddress(e->ether_shost), AddrInfo(source, p->timestamp_anno()));

	// Set outport if dst is unicast, we have info about it, and the
	// info is still valid.
	EtherAddress dst(e->ether_dhost);
	if (!dst.is_group()) {
	    if (Table::iterator dst_info = _table.find(dst)) {
		if (p->timestamp_anno() < dst_info.value().stamp + Timestamp(_timeout, 0))
		    outport = dst_info.value().port;
		else
		    _table.erase(dst_info);
	    }
	}
    }

    if (outport < 0)
	broadcast(source, p);
    else if (outport == source)	// Don't send back out on same interface
	output(noutputs() - 1).push(p);
    else {			// forward
	output(noutputs() - 1).push(p->clone());
	output(outport).push(p);
    }
}

ELEMENT_REQUIRES(EtherSwitch)
EXPORT_ELEMENT(ListenEtherSwitch)
CLICK_ENDDECLS
