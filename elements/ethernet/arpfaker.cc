/*
 * arpfaker.{cc,hh} -- ARP response faker element
 * Robert Morris
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "arpfaker.hh"
#include "click_ether.h"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

ARPFaker::ARPFaker()
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
ARPFaker::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  if(args.size() != 4){
    errh->error("usage: ARPFaker(ip1, eth1, ip2, eth2)");
    return(-1);
  }
  
  if (cp_ip_address(args[0], _ip1) &&
      cp_ethernet_address(args[1], _eth1) &&
      cp_ip_address(args[2], _ip2) &&
      cp_ethernet_address(args[3], _eth2)){
    /* yow */
  } else {
    errh->error("ARPFaker configuration expected ip and ether addr");
    return -1;
  }
  
  return 0;
}

int
ARPFaker::initialize(ErrorHandler *)
{
  timer_schedule_after_ms(1 * 1000); // Send an ARP reply periodically.
  return 0;
}

void
ARPFaker::run_scheduled()
{
  output(0).push(make_response(_eth1.data(),
                               _ip1.data(),
                               _eth2.data(),
                               _ip2.data()));
  timer_schedule_after_ms(10 * 1000);
}

Packet *
ARPFaker::make_response(u_char tha[6], /* him */
                            u_char tpa[4],
                            u_char sha[6], /* me */
                            u_char spa[4])
{
  struct ether_header *e;
  struct ether_arp *ea;
  Packet *q = Packet::make(sizeof(*e) + sizeof(*ea));
  memset(q->data(), '\0', q->length());
  e = (struct ether_header *) q->data();
  ea = (struct ether_arp *) (e + 1);
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
