/*
 * setipaddress.{cc,hh} -- element sets destination address annotation
 * to a particular IP address
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2008 Meraki, Inc.
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
#include "setipaddress.hh"
#include <click/args.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

SetIPAddress::SetIPAddress()
{
}

SetIPAddress::~SetIPAddress()
{
}

int
SetIPAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int anno = 0;
    if (Args(conf, this, errh)
        .read_mp("IPADDR", _ip)
        .read("ANNO", AnnoArg(4), anno)
        .complete() < 0)
        return -1;
    _anno = anno;
    return 0;
}

Packet *
SetIPAddress::simple_action(Packet *p)
{
    p->set_anno_u32(_anno, _ip.addr());
    return p;
}

void
SetIPAddress::add_handlers()
{
    add_data_handlers("addr", Handler::OP_READ | Handler::OP_WRITE, &_ip);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetIPAddress)
ELEMENT_MT_SAFE(SetIPAddress)
