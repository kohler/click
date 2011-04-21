/*
 * ackresponder.{cc,hh} -- element sends link-level ACKs
 * Douglas S. J. De Couto
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
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/args.hh>
#include <click/packet.hh>
#include "ackresponder.hh"
CLICK_DECLS

ACKResponder::ACKResponder()
{
}

ACKResponder::~ACKResponder()
{
}

Packet *
ACKResponder::simple_action(Packet *p)
{
  click_ether *e = (click_ether *) p->data();
  EtherAddress dest(e->ether_dhost);
  if (dest == _eth) {
    WritablePacket *xp = Packet::make(sizeof(click_ether) + 2); // +2 to align ether hdr
    xp->pull(2);
    click_ether *eth = (click_ether *) xp->data();
    eth->ether_type = htons(ETHERTYPE_GRID_ACK);
    memcpy(eth->ether_shost, e->ether_dhost, 6);
    memcpy(eth->ether_dhost, e->ether_shost, 6);
    output(1).push(xp);
  }
  return p;
}

int
ACKResponder::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("ETH", _eth).complete();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ACKResponder)
