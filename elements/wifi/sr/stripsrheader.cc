/*
 * stripsrheader.{cc,hh} -- element removes SR header
 * John Bicket
 * sampled from stripipheader.cc by Eddie Kohler
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
#include "stripsrheader.hh"
#include "srpacket.hh"
#include <clicknet/ether.h>
CLICK_DECLS

StripSRHeader::StripSRHeader()
  : Element(1, 1)
{
}

StripSRHeader::~StripSRHeader()
{
}

Packet *
StripSRHeader::simple_action(Packet *p)
{

  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);

  int extra = pk->hlen_wo_data() + sizeof(click_ether);
  p->pull(extra);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StripSRHeader)
ELEMENT_MT_SAFE(StripSRHeader)
