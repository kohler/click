/*
 * arpfaker6.{cc,hh} -- ARP response faker element
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "arpfaker6.hh"
#include "click_ether.h"
#include "etheraddress.hh"
#include "ip6address.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

ARPFaker6::ARPFaker6()
{
  add_output();
}

ARPFaker6::~ARPFaker6()
{
}

ARPFaker6 *
ARPFaker6::clone() const
{
  return new ARPFaker6;
}

int
ARPFaker6::configure(const Vector<String> &conf, ErrorHandler *errh)
{
return cp_va_parse(conf, this, errh,
		     cpIP6Address, "target IP6 address", &_ip1,
		     cpEthernetAddress, "target Ethernet address", &_eth1,
		     cpIP6Address, "sender IP6 address", &_ip2,
		     cpEthernetAddress, "sender Ethernet address", &_eth2,
		     0);
}

int
ARPFaker6::initialize(ErrorHandler *)
{
  _timer.attach(this);
  _timer.schedule_after_ms(1 * 1000); // Send an ARP reply periodically.
  return 0;
}

void
ARPFaker6::run_scheduled()
{
  output(0).push(make_response(_eth1.data(),
                               (u_char *)_ip1.data(),
                               _eth2.data(),
                               (u_char *)_ip2.data()));
  _timer.schedule_after_ms(10 * 1000);
}

Packet *
ARPFaker6::make_response(u_char tha[6], /* him */
                            u_char tpa[16],
                            u_char sha[6], /* me */
                            u_char spa[16])
{
  click_ether *e;
  click_ether_arp6 *ea;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*ea));
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ea = (click_ether_arp6 *) (e + 1);
  memcpy(e->ether_dhost, tha, 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_ARP);
  ea->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
  ea->ea_hdr.ar_pro = htons(ETHERTYPE_IP6);
  ea->ea_hdr.ar_hln = 6;
  ea->ea_hdr.ar_pln = 16;
  ea->ea_hdr.ar_op = htons(ARPOP_REPLY);
  memcpy(ea->arp_tha, tha, 6);
  memcpy(ea->arp_tpa, tpa, 16);
  memcpy(ea->arp_sha, sha, 6);
  memcpy(ea->arp_spa, spa, 16);
  return q;
}

EXPORT_ELEMENT(ARPFaker6)
