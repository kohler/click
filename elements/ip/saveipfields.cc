/*
 * saveipfields.{cc,hh} -- set IP TOS, TTL, and OFF annotations based on values
 * in packet data
 * Alex Snoeren, Eddie Kohler
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
#include "saveipfields.hh"
#include <click/click_ip.h>
#include <click/confparse.hh>
#include <click/error.hh>

SaveIPFields::SaveIPFields()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

SaveIPFields::~SaveIPFields()
{
  MOD_DEC_USE_COUNT;
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
ELEMENT_MT_SAFE(SaveIPFields)
