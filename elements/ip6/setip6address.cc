/*
 * setip6address.{cc,hh} -- element sets destination address annotation
 * to a particular IP6 address
 * Eddie Kohler, Peilei Fan
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "setip6address.hh"
#include <click/args.hh>
CLICK_DECLS

SetIP6Address::SetIP6Address()
{
}

SetIP6Address::~SetIP6Address()
{
}

int
SetIP6Address::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("IPADDR", _ip6).complete();
}

Packet *
SetIP6Address::simple_action(Packet *p)
{
    SET_DST_IP6_ANNO(p, _ip6);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetIP6Address)
