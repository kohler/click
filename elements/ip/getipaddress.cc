/*
 * getipaddress.{cc,hh} -- element sets IP destination annotation from
 * packet header
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2008-2011 Meraki, Inc.
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
#include "getipaddress.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
CLICK_DECLS

GetIPAddress::GetIPAddress()
{
}

GetIPAddress::~GetIPAddress()
{
}

int
GetIPAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _offset = -1;
    _anno = Packet::dst_ip_anno_offset;
    String ip_word;
    if (Args(conf, this, errh)
        .read_p("OFFSET", _offset)
        .read_p("ANNO", AnnoArg(4), _anno)
        .read("IP", ip_word)
        .complete())
	return -1;
    if ((_offset >= 0 && ip_word) || (_offset < 0 && !ip_word))
	return errh->error("set one of OFFSET, IP");
    else if (ip_word == "src")
	_offset = offset_ip_src;
    else if (ip_word == "dst")
	_offset = offset_ip_dst;
    else if (ip_word)
	return errh->error("bad IP");
    return 0;
}

Packet *
GetIPAddress::simple_action(Packet *p)
{
    if (_offset >= 0)
	p->set_anno_u32(_anno, IPAddress(p->data() + _offset).addr());
    else if (_offset == offset_ip_src)
	p->set_anno_u32(_anno, p->ip_header()->ip_src.s_addr);
    else if (_offset == offset_ip_dst)
	p->set_anno_u32(_anno, p->ip_header()->ip_dst.s_addr);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GetIPAddress)
ELEMENT_MT_SAFE(GetIPAddress)
