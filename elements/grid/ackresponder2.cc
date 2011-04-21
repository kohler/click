/*
 * ackresponder2.{cc,hh} -- element sends link-level ACKs
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include "ackresponder2.hh"
CLICK_DECLS

ACKResponder2::ACKResponder2()
{
}

ACKResponder2::~ACKResponder2()
{
}

Packet *
ACKResponder2::simple_action(Packet *p)
{
  IPAddress src(p->data());
  IPAddress dst(p->data() + 4);

  if (dst == _ip) {
    WritablePacket *xp = Packet::make(8);
    memcpy(xp->data(), _ip.data(), 4);
    memcpy(xp->data() + 4, src.data(), 4);
    output(1).push(xp);
  }
  p->pull(8);
  return p;
}

int
ACKResponder2::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("IP", _ip).complete();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ACKResponder2)
