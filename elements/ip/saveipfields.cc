/*
 * saveipfields.{cc,hh} -- set IP TOS, TTL, and OFF annotations based on values
 * in packet data
 * Alex Snoeren, Eddie Kohler
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "saveipfields.hh"
#include <click/click_ip.h>
#include <click/confparse.hh>
#include <click/error.hh>

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

ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(SaveIPFields)
