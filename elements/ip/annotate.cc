/*
 * annotate.{cc,hh} -- set IP TOS, TTL, and OFF annotations based on values
 * in packet data
 * Alex Snoeren
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
#include "annotate.hh"
#include "click_ip.h"
#include "confparse.hh"
#include "error.hh"

Annotate::Annotate()
  : Element(1, 1)
{
}

Packet *
Annotate::simple_action(Packet *p)
{
  struct ip *ip = (struct ip *) p->data();
  p->set_ip_tos_anno(ip->ip_tos);
  p->set_ip_ttl_anno(ip->ip_ttl);
  p->set_ip_off_anno(ip->ip_off);
  return p;
}

Annotate *
Annotate::clone() const
{
  return new Annotate();
}

EXPORT_ELEMENT(Annotate)
