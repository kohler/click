/*
 * hostetherfilter.{cc,hh} -- Discard packets not for this host.
 * Robert Morris
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

#include <click/config.h>
#include <click/package.hh>
#include "hostetherfilter.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <click/click_ether.h>

HostEtherFilter::HostEtherFilter()
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

HostEtherFilter::~HostEtherFilter()
{
  MOD_DEC_USE_COUNT;
}

void
HostEtherFilter::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
HostEtherFilter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  bool drop_own = false, drop_other = true;
  if (cp_va_parse(conf, this, errh,
		  cpEthernetAddress, "Ethernet address", &_addr,
		  cpOptional,
		  cpBool, "Drop packets from us?", &drop_own,
		  cpBool, "Drop packets to others?", &drop_other,
		  cpKeywords,
		  "DROP_OWN", cpBool, "Drop packets from us?", &drop_own,
		  "DROP_OTHER", cpBool, "Drop packets to others?", &drop_other,
		  0) < 0)
    return -1;
  _drop_own = drop_own;
  _drop_other = drop_other;
  return 0;
}

HostEtherFilter *
HostEtherFilter::clone() const
{
  return new HostEtherFilter;
}

Packet *
HostEtherFilter::drop(Packet *p)
{
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
  return 0;
}

Packet *
HostEtherFilter::simple_action(Packet *p)
{
  const click_ether *e = (const click_ether *)p->data();
  const unsigned short *daddr = (const unsigned short *)e->ether_dhost;

  if (_drop_own && memcmp(e->ether_shost, _addr, 6) == 0)
    return drop(p);
  else if (memcmp(e->ether_dhost, _addr, 6) == 0) {
    p->set_packet_type_anno(Packet::HOST);
    return p;
  } else if (daddr[0] == 0xFFFF && daddr[1] == 0xFFFF && daddr[2] == 0xFFFF) {
    p->set_packet_type_anno(Packet::BROADCAST);
    return p;
  } else if (e->ether_dhost[0] & 0x80) {
    p->set_packet_type_anno(Packet::MULTICAST);
    return p;
  } else {
    p->set_packet_type_anno(Packet::OTHERHOST);
    return (_drop_other ? drop(p) : p);
  }
}

EXPORT_ELEMENT(HostEtherFilter)
