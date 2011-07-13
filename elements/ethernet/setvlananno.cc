/*
 * setvlananno.{cc,hh} -- set VLAN annotation
 *
 * Copyright (c) 2010 Intel Corporation
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
#include "setvlananno.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
CLICK_DECLS

SetVLANAnno::SetVLANAnno()
{
}

SetVLANAnno::~SetVLANAnno()
{
}

int
SetVLANAnno::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int tci = -1, id = 0, pcp = 0;
    if (Args(conf, this, errh)
	.read_p("VLAN_TCI", BoundedIntArg(0, 0xFFFF), tci)
	.read_p("VLAN_PCP", BoundedIntArg(0, 7), pcp)
	.read("VLAN_ID", BoundedIntArg(0, 0xFFF), id)
	.complete() < 0)
	return -1;
    _vlan_tci = htons((tci >= 0 ? tci : id) | (pcp << 13));
    return 0;
}

Packet *
SetVLANAnno::simple_action(Packet *p)
{
    SET_VLAN_TCI_ANNO(p, _vlan_tci);
    return p;
}

String
SetVLANAnno::read_handler(Element *e, void *user_data)
{
    SetVLANAnno *sva = static_cast<SetVLANAnno *>(e);
    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_config: {
	StringAccum sa;
	sa << "VLAN_ID " << (ntohs(sva->_vlan_tci) & 0xFFF)
	   << ", VLAN_PCP " << ((ntohs(sva->_vlan_tci) >> 13) & 7);
	return sa.take_string();
    }
    case h_vlan_tci:
	return String(ntohs(sva->_vlan_tci));
    }
    return String();
}

void
SetVLANAnno::add_handlers()
{
    add_read_handler("config", read_handler, h_config);
    add_read_handler("vlan_tci", read_handler, h_vlan_tci);
    add_write_handler("vlan_tci", reconfigure_keyword_handler, "VLAN_TCI");
    add_read_handler("vlan_id", read_keyword_handler, "VLAN_ID");
    add_write_handler("vlan_id", reconfigure_keyword_handler, "VLAN_ID");
    add_read_handler("vlan_pcp", read_keyword_handler, "VLAN_PCP");
    add_write_handler("vlan_pcp", reconfigure_keyword_handler, "VLAN_PCP");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetVLANAnno SetVLANAnno-SetVlanAnno)
