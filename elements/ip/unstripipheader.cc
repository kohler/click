/*
 * unstripipheader.{cc,hh} -- put IP header back based on annotation
 * Benjie Chen
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
#include "unstripipheader.hh"
#include <clicknet/ip.h>
CLICK_DECLS

UnstripIPHeader::UnstripIPHeader()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

UnstripIPHeader::~UnstripIPHeader()
{
  MOD_DEC_USE_COUNT;
}

Packet *
UnstripIPHeader::simple_action(Packet *p)
{
  int offset = (unsigned char*) p->ip_header() - p->data();
  if (offset < 0) 
    p->push(0-offset);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(UnstripIPHeader)
ELEMENT_MT_SAFE(UnstripIPHeader)
