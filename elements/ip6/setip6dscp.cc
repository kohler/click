/*
 * setip6dscp.{cc,hh} -- element sets IP6 header DSCP field
 * Frederik Scholaert
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
#include "setip6dscp.hh"
#include <clicknet/ip6.h>
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

SetIP6DSCP::SetIP6DSCP()
{
}

SetIP6DSCP::~SetIP6DSCP()
{
}

int
SetIP6DSCP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned dscp_val;
    if (Args(conf, this, errh).read_mp("DSCP", dscp_val).complete() < 0)
	return -1;
    if (dscp_val > 0x3F)
	return errh->error("diffserv code point out of range");

    // OK: set values
    _dscp = htonl(dscp_val << IP6_DSCP_SHIFT);
    return 0;
}


inline Packet *
SetIP6DSCP::smaction(Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    assert(p->has_network_header());
    click_ip6 *ip6 = p->ip6_header();
    ip6->ip6_flow = (ip6->ip6_flow & htonl(~IP6_DSCP_MASK)) | _dscp;
    return p;
}

void
SetIP6DSCP::push(int, Packet *p)
{
  if ((p = smaction(p)) != 0)
    output(0).push(p);
}

Packet *
SetIP6DSCP::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    p = smaction(p);
  return p;
}

void
SetIP6DSCP::add_handlers()
{
    add_read_handler("dscp", read_keyword_handler, "0 DSCP", Handler::CALM);
    add_write_handler("dscp", reconfigure_keyword_handler, "0 DSCP");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetIP6DSCP)
ELEMENT_MT_SAFE(SetIP6DSCP)
