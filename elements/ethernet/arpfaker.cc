/*
 * arpfaker.{cc,hh} -- ARP response faker element
 * Robert Morris
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
#include "arpfaker.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

ARPFaker::ARPFaker()
  : _timer(this)
{
  MOD_INC_USE_COUNT;
  add_output();
}

ARPFaker::~ARPFaker()
{
  MOD_DEC_USE_COUNT;
}

ARPFaker *
ARPFaker::clone() const
{
  return new ARPFaker;
}

int
ARPFaker::configure(Vector<String> &conf, ErrorHandler *errh)
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
  _timer.initialize(this);
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
  q->set_mac_header(q->data(), 14);
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

CLICK_ENDDECLS
EXPORT_ELEMENT(ARPFaker)
