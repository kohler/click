/*
 * saveipfields.{cc,hh} -- set IP TOS, TTL, and OFF annotations based on values
 * in packet data
 * Alex Snoeren, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "saveipfields.hh"
#include "click_ip.h"
#include "confparse.hh"
#include "error.hh"

SaveIPFields::SaveIPFields()
  : Element(1, 1)
{
}

SaveIPFields *
SaveIPFields::clone() const
{
  return new SaveIPFields;
}

Packet *
SaveIPFields::simple_action(Packet *p)
{
  const click_ip *ip = p->ip_header();
  assert(ip);
  p->set_ip_tos_anno(ip->ip_tos);
  p->set_ip_ttl_anno(ip->ip_ttl);
  p->set_ip_off_anno(ip->ip_off);
  return p;
}

EXPORT_ELEMENT(SaveIPFields)
