/*
 * arpfaker.{cc,hh} -- ARP response faker element
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "arpfaker.hh"
#include <click/click_ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

ARPFaker::ARPFaker()
  : _timer(this)
{
  add_output();
}

ARPFaker::~ARPFaker()
{
}

ARPFaker *
ARPFaker::clone() const
{
  return new ARPFaker;
}

int
ARPFaker::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpIPAddress, "target IP address", &_ip1,
		     cpEthernetAddress, "target Ethernet address", &_eth1,
		     cpIPAddress, "sender IP address", &_ip2,
		     cpEthernetAddress, "sender Ethernet address", &_eth2,
		     0);
}

int
ARPFaker::initialize(ErrorHandler *)
{
  _timer.attach(this);
  _timer.schedule_after_ms(1 * 1000); // Send an ARP reply periodically.
  return 0;
}

void
ARPFaker::run_scheduled()
{
  output(0).push(make_response(_eth1.data(),
                               _ip1.data(),
                               _eth2.data(),
                               _ip2.data()));
  _timer.schedule_after_ms(10 * 1000);
}

Packet *
ARPFaker::make_response(u_char tha[6], /* him */
                            u_char tpa[4],
                            u_char sha[6], /* me */
                            u_char spa[4])
{
  click_ether *e;
  click_ether_arp *ea;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*ea));
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ea = (click_ether_arp *) (e + 1);
  memcpy(e->ether_dhost, tha, 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_ARP);
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP);
  ea->ea_hdr.ar_hln = 6;
  ea->ea_hdr.ar_pln = 4;
  ea->ea_hdr.ar_op = htons(ARPOP_REPLY);
  memcpy(ea->arp_tha, tha, 6);
  memcpy(ea->arp_tpa, tpa, 4);
  memcpy(ea->arp_sha, sha, 6);
  memcpy(ea->arp_spa, spa, 4);
  return q;
}

EXPORT_ELEMENT(ARPFaker)
