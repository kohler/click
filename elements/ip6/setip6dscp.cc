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
#include <click/click_ip6.h>
#include <click/confparse.hh>
#include <click/error.hh>

SetIP6DSCP::SetIP6DSCP()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SetIP6DSCP::~SetIP6DSCP()
{
  MOD_DEC_USE_COUNT;
}

SetIP6DSCP *
SetIP6DSCP::clone() const
{
  return new SetIP6DSCP;
}

int
SetIP6DSCP::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned dscp_val;
  if (cp_va_parse(conf, this, errh,
                  cpUnsigned, "diffserv code point", &dscp_val,
                  0) < 0)
    return -1;
  if (dscp_val > 0x3F)
    return errh->error("diffserv code point out of range");

  // OK: set values
  _dscp = (dscp_val << IP6_DSCP_SHIFT);
  return 0;
}


inline Packet *
SetIP6DSCP::smaction(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip6 *ip6 = p->ip6_header();
  assert(ip6);

  uint32_t flow = ntohl(ip6->ip6_flow);
  ip6->ip6_flow = htonl((flow & ~IP6_DSCP_MASK) | _dscp);

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

static String
SetIP6DSCP_read_dscp(Element *xf, void *)
{
  SetIP6DSCP *f = (SetIP6DSCP *)xf;
  return String((int)f->dscp()) + "\n";
}

void
SetIP6DSCP::add_handlers()
{
  add_read_handler("dscp", SetIP6DSCP_read_dscp, (void *)0);
  add_write_handler("dscp", reconfigure_positional_handler, (void *)0);
}

EXPORT_ELEMENT(SetIP6DSCP)
ELEMENT_MT_SAFE(SetIP6DSCP)
