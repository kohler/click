/*
 * ip6ndadvertiser.{cc,hh} -- element that responds to
 * Neighbor Solitation Msg
 * Peilei Fan
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "ip6ndadvertiser.hh"
#include <clicknet/ether.h>
#include <clicknet/ip6.h>
#include <click/etheraddress.hh>
#include <click/ip6address.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

IP6NDAdvertiser::IP6NDAdvertiser()
{
}

IP6NDAdvertiser::~IP6NDAdvertiser()
{
}

void
IP6NDAdvertiser::add_map(const IP6Address &ipa, const IP6Address &mask, const EtherAddress &ena)
{
  struct Entry e;
  e.dst = ipa;
  e.mask = mask;
  e.ena = ena;
  _v.push_back(e);
}

int
IP6NDAdvertiser::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _v.clear();

  for (int i = 0; i < conf.size(); i++) {
    IP6Address ipa, mask;
    EtherAddress ena;
    bool have_ena = false;
    int first = _v.size();

    Vector<String> words;
    cp_spacevec(conf[i], words);

    for (int j = 0; j < words.size(); j++)
      if (cp_ip6_prefix(words[j], (unsigned char *)&ipa, (unsigned char *)&mask, true, this))
	add_map(ipa, mask, EtherAddress());
      else if (cp_ip6_address(words[j], &ipa, this))
	add_map(ipa, IP6Address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"), EtherAddress());
      else if (EtherAddressArg().parse(words[j], ena, this)) {
	if (have_ena)
	  errh->error("argument %d has more than one Ethernet address", i);
	have_ena = true;
      } else {
	errh->error("argument %d should be `IP6/MASK ETHADDR'", i);
	j = words.size();
      }

    if (first == _v.size())
      errh->error("argument %d had no IP6 address and masks", i);
    for (int j = first; j < _v.size(); j++)
      _v[j].ena = ena;
  }

  return errh->nerrors() ? -1 : 0;
}


Packet *
IP6NDAdvertiser::make_response(u_char dha[6],   /*  des eth address */
			     u_char sha[6],   /*  src eth address */
			     u_char dpa[16],  /*  dst IP6 address */
			     u_char spa[16],  /*  src IP6 address */
			     u_char tpa[16],  /*  target IP6 address */
			     u_char tha[6])   /*  target eth address */

{
  click_ether *e;
  click_ip6 *ip6;
  click_nd_adv *ea;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*ip6)+ sizeof(*ea));
  if (q == 0) {
    click_chatter("in NDadv: cannot make packet!");
    assert(0);
  }
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ip6=(click_ip6 *) (e+1);
  ea = (click_nd_adv *) (ip6 + 1);

  //set ethernet header
  memcpy(e->ether_dhost, dha, 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_IP6);

  //set ip6 header
  ip6->ip6_flow = 0;		// set flow to 0 (includes version)
  ip6->ip6_v = 6;		// then set version to 6
  ip6->ip6_plen = htons(sizeof(click_nd_adv));
  ip6->ip6_nxt=0x3a; //i.e. protocal: icmp6 message
  ip6->ip6_hlim=0xff; //indicate no router has processed it
  ip6->ip6_src = IP6Address(spa);
  ip6->ip6_dst = IP6Address(dpa);


  //set Neighborhood Solicitation Validation Message
  ea->type = 0x88;
  ea->code =0;

  // fixed from 0x60 thanks to Simona Fischera
  ea->flags = 0xC0; //  this is the same as setting the following
                    //  ea->sender_is_router = 1;
                    //  ea->solicited =1;
                    //  ea->override = 0;

  for (int i=0; i<3; i++) {
    ea->reserved[i] = 0;
  }

  memcpy(ea->nd_tpa, tpa, 16);
  ea->option_type = 0x2;
  ea->option_length = 0x1;
  memcpy(ea->nd_tha, tha, 6);
  ea->checksum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, 0, (unsigned char *)ea, ip6->ip6_plen));

  return q;
}

Packet *
IP6NDAdvertiser::make_response2(u_char dha[6],   /*  des eth address */
			     u_char sha[6],   /*  src eth address */
			     u_char dpa[16],  /*  dst IP6 address */
			     u_char spa[16],  /*  src IP6 address */
			     u_char tpa[16])  /*  target IP6 address */

{
  click_ether *e;
  click_ip6 *ip6;
  click_nd_adv2 *ea;
  WritablePacket *q = Packet::make(sizeof(*e) + sizeof(*ip6)+ sizeof(*ea));
  if (q == 0) {
    click_chatter("in IP6NDAdvertiser: cannot make packet!");
    assert(0);
  }
  memset(q->data(), '\0', q->length());
  e = (click_ether *) q->data();
  ip6=(click_ip6 *) (e+1);
  ea = (click_nd_adv2 *) (ip6 + 1);

  //set ethernet header
  memcpy(e->ether_dhost, dha, 6);
  memcpy(e->ether_shost, sha, 6);
  e->ether_type = htons(ETHERTYPE_IP6);

  //set ip6 header
  ip6->ip6_flow = 0;		// set flow to 0 (includes version)
  ip6->ip6_v = 6;		// then set version to 6
  ip6->ip6_plen=htons(sizeof(click_nd_adv2));
  ip6->ip6_nxt=0x3a; //i.e. protocal: icmp6 message
  ip6->ip6_hlim=0xff; //indicate no router has processed it
  ip6->ip6_src = IP6Address(spa);
  ip6->ip6_dst = IP6Address(dpa);


  //set Neighbor Solicitation Validation Message
  ea->type = 0x88;
  ea->code =0;

  // fixed from 0x60 thanks to Simona Fischera
  ea->flags = 0xC0; //  this is the same as setting the following
                    //  ea->sender_is_router = 1;
                    //  ea->solicited =1;
                    //  ea->override = 0;

  for (int i=0; i<3; i++) {
    ea->reserved[i] = 0;
  }

  memcpy(ea->nd_tpa, tpa, 16);

  ea->checksum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, 0, (unsigned char *)(ip6+1), htons(sizeof(click_nd_adv2))));

  return q;
}


bool
IP6NDAdvertiser::lookup(const IP6Address &a, EtherAddress &ena) const
{
  int best = -1;
  for (int i = 0; i < _v.size(); i++)
    if (a.matches_prefix(_v[i].dst, _v[i].mask)) {
      if (best < 0 || _v[i].mask.mask_as_specific(_v[best].mask))
	best = i;
    }

  if (best < 0)
    return false;
  else {
    ena = _v[best].ena;
    return true;
  }
}

Packet *
IP6NDAdvertiser::simple_action(Packet *p)
{
   click_ether *e = (click_ether *) p->data();
   click_ip6 *ip6 = (click_ip6 *) (e + 1);
   click_nd_sol *ea=(click_nd_sol *) (ip6 + 1);
   unsigned char tpa[16];
   unsigned char spa[16];
   unsigned char dpa[16];
   memcpy(&tpa, ea->nd_tpa, 16);
   memcpy(&dpa, IP6Address(ip6->ip6_src).data(), 16);
   IP6Address ipa = IP6Address(tpa);

   //check see if the packet is corrupted by recalculating its checksum
   unsigned short int csum2 = in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, ea->checksum, (unsigned char *)(ip6+1), htons(sizeof(click_nd_sol)));

   Packet *q = 0;

  if (p->length() >= sizeof(*e) + sizeof(click_ip6) + sizeof(click_nd_sol) &&
      ntohs(e->ether_type) == ETHERTYPE_IP6 &&
      ip6->ip6_hlim ==0xff &&
      ea->type == ND_SOL &&
      ea->code == 0 &&
      csum2 == ntohs(ea->checksum))
    {
      EtherAddress ena;
      unsigned char host_ether[6];
      if(lookup(ipa, ena))
	{
	  memcpy(&spa, ipa.data(), 16);
	  memcpy(&host_ether, ena.data(),6);

	  //use the finded ip6address as its source ip6 address in the
	  //header in neighborhood advertisement message packet
	  q = make_response(e->ether_shost, host_ether, dpa, spa, tpa, host_ether);

	}
    }

  else
    {
      struct in6_addr ina;
      memcpy(&ina, &ea->nd_tpa, 16);
    }

  p->kill();
  return(q);
}

ELEMENT_REQUIRES(ip6)
EXPORT_ELEMENT(IP6NDAdvertiser)
CLICK_ENDDECLS
