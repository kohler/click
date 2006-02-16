/*
 * sr2stripheader.{cc,hh} -- element removes SR header
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
#include "sr2stripheader.hh"
#include "sr2packet.hh"
#include <clicknet/ether.h>
CLICK_DECLS

SR2StripHeader::SR2StripHeader()
{
}

SR2StripHeader::~SR2StripHeader()
{
}

Packet *
SR2StripHeader::simple_action(Packet *p)
{

  click_ether *eh = (click_ether *) p->data();
  struct sr2packet *pk = (struct sr2packet *) (eh+1);

  int extra = pk->hlen_wo_data() + sizeof(click_ether);
  p->pull(extra);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SR2StripHeader)
ELEMENT_MT_SAFE(SR2StripHeader)
