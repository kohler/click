/*
 * setipdscp.{cc,hh} -- element sets IP header DSCP field
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "setipdscp.hh"
#include "click_ip.h"
#include "confparse.hh"
#include "error.hh"

SetIPDSCP::SetIPDSCP()
{
  add_input();
  add_output();
}

SetIPDSCP *
SetIPDSCP::clone() const
{
  return new SetIPDSCP;
}

int
SetIPDSCP::configure(const String &conf, ErrorHandler *errh)
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
SetIPDSCP::smaction(Packet *p)
{
  click_ip *ip = p->ip_header();
  assert(ip);
  
  unsigned short old_hw = ((unsigned short *)ip)[0];
  ip->ip_tos = (ip->ip_tos & 0x3) | _dscp;
  unsigned short new_hw = ((unsigned short *)ip)[0];
  
  // 19.Aug.1999 - incrementally update IP checksum according to RFC1624.
  // new_sum = ~(~old_sum + ~old_halfword + new_halfword)
  unsigned long sum =
    (~ntohs(ip->ip_sum) & 0xFFFF) + (~ntohs(old_hw) & 0xFFFF) + ntohs(new_hw);
  ip->ip_sum = ~htons(sum + (sum >> 16));
  
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
  return String(f->dscp()) + "\n";
}

void
SetIPDSCP::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read_write("value", SetIPDSCP_read_dscp, (void *)0,
		      reconfigure_write_handler, (void *)0);
}

EXPORT_ELEMENT(SetIPDSCP)
