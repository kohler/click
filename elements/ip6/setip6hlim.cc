/*
 * setip6hlim.{cc,hh} -- element sets IP6 hop limit field
 * Glenn Minne
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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
#include "setip6hlim.hh"
#include <clicknet/ip6.h>
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

SetIP6Hlim::SetIP6Hlim()
{
}

SetIP6Hlim::~SetIP6Hlim()
{
}

int
SetIP6Hlim::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh).read_mp("HLIM", _hlim).complete() < 0)
	    return -1;
	    
    return 0;
}


inline Packet *
SetIP6Hlim::smaction(Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    assert(p->has_network_header());
    click_ip6 *ip6 = p->ip6_header();
    
    ip6->ip6_ctlun.ip6_un1.ip6_un1_hlim = _hlim;

    return p;
}

void
SetIP6Hlim::push(int, Packet *p)
{
    if ((p = smaction(p)) != 0)
        output(0).push(p);
}

Packet *
SetIP6Hlim::pull(int)
{
    Packet *p = input(0).pull();
    if (p)
        p = smaction(p);
    return p;
}

void
SetIP6Hlim::add_handlers()
{
    add_read_handler("hopLimit", read_keyword_handler, "0 SetIP6Hlim", Handler::CALM);
    add_write_handler("hopLimit", reconfigure_keyword_handler, "0 SetIP6Hlim");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetIP6Hlim)
ELEMENT_MT_SAFE(SetIP6Hlim)
