/*
 * ensureether.{cc,hh} -- ensures that IP packets are Ethernet-encapsulated
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include "ensureether.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

EnsureEther::EnsureEther()
{
}

EnsureEther::~EnsureEther()
{
}

int
EnsureEther::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned etht = 0x0800;
    memset(&_ethh.ether_shost, 1, 6);
    memset(&_ethh.ether_dhost, 2, 6);
    if (Args(conf, this, errh)
	.read_p("ETHERTYPE", etht)
	.read_p("SRC", EtherAddressArg(), _ethh.ether_shost)
	.read_p("DST", EtherAddressArg(), _ethh.ether_dhost)
	.complete() < 0)
	return -1;
    if (etht > 0xFFFF)
	return errh->error("argument 1 (Ethernet encapsulation type) must be <= 0xFFFF");
    _ethh.ether_type = htons(etht);
    return 0;
}

Packet *
EnsureEther::smaction(Packet *p)
{
  if (!p->has_network_header() || p->ip_header_offset() < 0)
    return p;

  if (p->ip_header_offset() == 14) {
    // check for an existing Ethernet header
    const click_ether *ethh = (const click_ether *)p->data();
    if (ethh->ether_type == htons(ETHERTYPE_IP)
	|| ethh->ether_type == htons(ETHERTYPE_IP6))
      return p;
  } else if (p->ip_header_offset() == 0 && p->headroom() >= 14) {
    // check for an Ethernet header that had been stripped
    const click_ether *ethh = (const click_ether *)(p->data() - 14);
    if (ethh->ether_type == htons(ETHERTYPE_IP)
	|| ethh->ether_type == htons(ETHERTYPE_IP6))
      return p->nonunique_push(14);
  }

  // need to prepend our own Ethernet header
  p->pull(p->ip_header_offset());
  if (WritablePacket *q = p->push(14)) {
    memcpy(q->data(), &_ethh, 14);
    return q;
  } else
    return 0;
}

void
EnsureEther::push(int, Packet *p)
{
  if (Packet *q = smaction(p))
    output(0).push(q);
}

Packet *
EnsureEther::pull(int)
{
  if (Packet *p = input(0).pull())
    return smaction(p);
  else
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EnsureEther)
