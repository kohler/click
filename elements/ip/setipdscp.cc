/*
 * setipdscp.{cc,hh} -- element sets IP header DSCP field
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include "setipdscp.hh"
#include <click/click_ip.h>
#include <click/confparse.hh>
#include <click/error.hh>

SetIPDSCP::SetIPDSCP()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SetIPDSCP::~SetIPDSCP()
{
  MOD_DEC_USE_COUNT;
}

SetIPDSCP *
SetIPDSCP::clone() const
{
  return new SetIPDSCP;
}

int
SetIPDSCP::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned dscp_val;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "diffserv code point", &dscp_val,
		  0) < 0)
    return -1;
  if (dscp_val > 0x3F)
    return errh->error("diffserv code point out of range");

  // OK: set values
  _dscp = (dscp_val << 2);
  return 0;
}

inline Packet *
SetIPDSCP::smaction(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  click_ip *ip = p->ip_header();
  assert(ip);
  
  unsigned short old_hw = ((unsigned short *)ip)[0];
  ip->ip_tos = (ip->ip_tos & 0x3) | _dscp;
  unsigned short new_hw = ((unsigned short *)ip)[0];
  
  // 19.Aug.1999 - incrementally update IP checksum according to RFC1624.
  // new_sum = ~(~old_sum + ~old_halfword + new_halfword)
  unsigned sum = (~ip->ip_sum & 0xFFFF) + (~old_hw & 0xFFFF) + new_hw;
  sum = (sum & 0xFFFF) + (sum >> 16);
  ip->ip_sum = ~(sum + (sum >> 16));
  
  return p;
}

void
SetIPDSCP::push(int, Packet *p)
{
  if ((p = smaction(p)) != 0)
    output(0).push(p);
}

Packet *
SetIPDSCP::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    p = smaction(p);
  return p;
}

static String
SetIPDSCP_read_dscp(Element *xf, void *)
{
  SetIPDSCP *f = (SetIPDSCP *)xf;
  return String((int)f->dscp()) + "\n";
}

void
SetIPDSCP::add_handlers()
{
  add_read_handler("dscp", SetIPDSCP_read_dscp, (void *)0);
  add_write_handler("dscp", reconfigure_positional_handler, (void *)0);
}

EXPORT_ELEMENT(SetIPDSCP)
ELEMENT_MT_SAFE(SetIPDSCP)
