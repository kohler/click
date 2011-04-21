/*
 * getip6address.{cc,hh} -- element sets IP6 destination annotation from
 * packet header
 * Peilei Fan
 *
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
#include "getip6address.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip6.h>
CLICK_DECLS

GetIP6Address::GetIP6Address()
{
}

GetIP6Address::~GetIP6Address()
{
}


int
GetIP6Address::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("OFFSET", _offset).complete();
}

Packet *
GetIP6Address::simple_action(Packet *p)
{

  IP6Address dst=IP6Address((unsigned char *)(p->data()+ _offset));
  SET_DST_IP6_ANNO(p, dst);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GetIP6Address)
