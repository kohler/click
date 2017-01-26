/*
 * protocoltranslator46{cc,hh} -- element that translates an IPv4 packet
 * to an IPv6 packet.
 * When translating an IPv6 packet to IPv6 packet, just simpley prefix the
 * ipv4 addresses with 96-bits prefix "0:0:0:0:ffff".
 *
 *
 * Peilei Fan
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#include <click/config.h>
#include "protocoltranslator46.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/ip6.h>
#include <clicknet/icmp.h>
#include <clicknet/icmp6.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
CLICK_DECLS

ProtocolTranslator46::ProtocolTranslator46()
{
}


ProtocolTranslator46::~ProtocolTranslator46()
{
}


//make the ipv4->ipv6 translation of the packet according to SIIT (RFC 2765)
Packet *
ProtocolTranslator46::make_translate46(IP6Address src,
				     IP6Address dst,
				     click_ip * ip,
				     unsigned char *a)
{

  WritablePacket *q = Packet::make(sizeof(click_ip6)-sizeof(click_ip)+ntohs(ip->ip_len));

  if (q==0) {
    click_chatter("can not make packet!");
    assert(0);
  }

  memset(q->data(), '\0', q->length());
  click_ip6 *ip6=(click_ip6 *)q->data();
  click_tcp *tcp = (click_tcp *)(ip6+1);
  click_udp *udp = (click_udp *)(ip6+1);

  //set ipv6 header
  ip6->ip6_flow = 0;	/* must set first: overlaps vfc */
  ip6->ip6_v = 6;
  ip6->ip6_plen = htons(ntohs(ip->ip_len)-sizeof(click_ip));
  ip6->ip6_hlim = ip->ip_ttl + 0x40-0xff;
  ip6->ip6_src = src;
  ip6->ip6_dst = dst;
  memcpy((unsigned char *)tcp, a, ntohs(ip6->ip6_plen));

  if (ip->ip_p == 6) //TCP
    {
      ip6->ip6_nxt = ip->ip_p;
      tcp->th_sum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, tcp->th_sum, a, ip6->ip6_plen));
    }

  else if (ip->ip_p == 17) //UDP
    {
      ip6->ip6_nxt = ip->ip_p;
      udp->uh_sum = htons(in6_fast_cksum(&ip6->ip6_src, &ip6->ip6_dst, ip6->ip6_plen, ip6->ip6_nxt, udp->uh_sum, a, ip6->ip6_plen));
    }

  else if (ip->ip_p == 1)
    {
      ip6->ip6_nxt=0x3a;
      //icmp 6->4 translation is dealt by caller.
    }

  else
    {
      //will deal other protocols later
    }

  return q;

}


Packet *
ProtocolTranslator46::make_icmp_translate46(IP6Address ip6_src,
					    IP6Address ip6_dst,
					    unsigned char *a,
					    unsigned char payload_length)
{
  click_ip *ip=0;
  unsigned char *ip6=0;
  unsigned char icmp_type = a[0];
  unsigned char icmp_code = a[1];
  unsigned char icmp_pointer = a[4];

  unsigned char icmp6_code = 0;
  unsigned char icmp6_length;

  WritablePacket *q2 = 0;

  switch (icmp_type)  {
  case (ICMP_ECHO): ; // icmp_type ==8
  case (ICMP_ECHOREPLY):  {//icmp_type ==0
    click_icmp_echo *icmp = (click_icmp_echo *)a;
    ip = (click_ip *)(icmp+1);
    icmp6_length = payload_length-sizeof(click_icmp_echo)+sizeof(click_icmp6_echo);
    q2=Packet::make(icmp6_length);
    memset(q2->data(), '\0', q2->length());
    click_icmp6_echo *icmp6 = (click_icmp6_echo *)q2->data();
    ip6=(unsigned char *)(icmp6+1);

    if (icmp_type == ICMP_ECHO ) { // icmp_type ==8
      icmp6->icmp6_type = ICMP6_ECHO;  // icmp6_type =128
    }
    else if (icmp_type == ICMP_ECHOREPLY ) { // icmp_type ==0
      icmp6->icmp6_type = ICMP6_ECHOREPLY;              // icmp6_type = 129
    }
    icmp6->icmp6_identifier = icmp->icmp_identifier;
    icmp6->icmp6_sequence = icmp->icmp_sequence;
    memcpy(ip6, (unsigned char *)ip, icmp6_length);
    icmp->icmp_cksum  = 0;
  icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.in6_addr(), &ip6_dst.in6_addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));

  }
  break;

  case (ICMP_UNREACH):  { //icmp_type ==3
    click_icmp_unreach *icmp = (click_icmp_unreach *)a;
    ip = (click_ip *)(icmp+1);

    if (icmp_code == 2 || icmp_code ==4) {
      switch (icmp_code) {
      case 2: {
	icmp6_length = payload_length-sizeof(click_icmp_unreach)+sizeof(click_icmp6_paramprob);
	q2=Packet::make(icmp6_length);
	memset(q2->data(), '\0', q2->length());
	click_icmp6_paramprob *icmp6 = (click_icmp6_paramprob *)q2->data();
	ip6=(unsigned char *)(icmp6+1);
	icmp6->icmp6_type = ICMP6_PARAMPROB; //icmp6_type = 4
	icmp6->icmp6_code = 1;
	icmp6->icmp6_pointer = 6;
	memcpy(ip6, (unsigned char *)ip, icmp6_length);
	icmp->icmp_cksum  = 0;
	icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.in6_addr(), &ip6_dst.in6_addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
      }
      break;

      case 4: {
	icmp6_length = payload_length-sizeof(click_icmp_unreach)+sizeof(click_icmp6_pkttoobig);
	q2=Packet::make(icmp6_length);
	memset(q2->data(), '\0', q2->length());
	click_icmp6_pkttoobig *icmp6 = (click_icmp6_pkttoobig *)q2->data();
	ip6=(unsigned char *)(icmp6+1);
	icmp6->icmp6_type = ICMP6_PKTTOOBIG; //icmp6_type = 2
	icmp6->icmp6_code = 0;

	//adjust the mtu field for the difference between the ipv4 and ipv6 header size

	memcpy(ip6, (unsigned char *)ip, icmp6_length);
	icmp->icmp_cksum  = 0;
	icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.in6_addr(), &ip6_dst.in6_addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
      }
      break;
      default: ;
      }
    }
    else {

      switch (icmp_code) {
      case 0 : ;
      case 1 : ;
      case 6 : ;
      case 7 : ;
      case 8 : ;
      case 11: ;
      case 12: {
	icmp6_code = 0;
      }
      break;
      case 3 : {
	icmp6_code = 4;
      }
      break;
      case 5 : {
	icmp6_code = 2;
      }
      break;
      case 9 : ;
      case 10: {
	icmp6_code = 1;
      }
      break;
      default:  
      break;
      }
      icmp6_length = payload_length-sizeof(click_icmp_unreach)+sizeof(click_icmp6_unreach);
      q2=Packet::make(icmp6_length);
      memset(q2->data(), '\0', q2->length());
      click_icmp6_unreach *icmp6 = (click_icmp6_unreach *)q2->data();
      ip6=(unsigned char *)(icmp6+1);
      icmp6->icmp6_type = ICMP6_UNREACH;
      icmp6->icmp6_code = icmp6_code;
      memcpy(ip6, (unsigned char *)ip, icmp6_length);
      icmp->icmp_cksum  = 0;
      icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.in6_addr(), &ip6_dst.in6_addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
    }
  }
  break;

  case (ICMP_TIMXCEED) : { //icmp ==11
    click_icmp_timxceed *icmp = (click_icmp_timxceed *)a;
    ip = (click_ip *)(icmp+1);
    icmp6_length = payload_length-sizeof(click_icmp_timxceed)+sizeof(click_icmp6_timxceed);
    q2=Packet::make(icmp6_length);
    memset(q2->data(), '\0', q2->length());
    click_icmp6_timxceed *icmp6 = (click_icmp6_timxceed *)q2->data();
    ip6=(unsigned char *)(icmp6+1);
    icmp6->icmp6_type=ICMP6_TIMXCEED;
    icmp6->icmp6_code = icmp_code;
    memcpy(ip6, (unsigned char *)ip, icmp6_length);
    icmp->icmp_cksum  = 0;
    icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.in6_addr(), &ip6_dst.in6_addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
  }
  break;

  case (ICMP_PARAMPROB): { //icmp==12
    click_icmp_paramprob *icmp = (click_icmp_paramprob *)a;
    ip = (click_ip *)(icmp+1);
    icmp6_length = payload_length-sizeof(click_icmp_paramprob)+sizeof(click_icmp6_paramprob);
    q2=Packet::make(icmp6_length);
    memset(q2->data(), '\0', q2->length());
    click_icmp6_paramprob *icmp6 = (click_icmp6_paramprob *)q2->data();
    ip6=(unsigned char *)(icmp6+1);
    icmp6->icmp6_type=ICMP6_PARAMPROB;

    switch (icmp_pointer) {
    case 0  : icmp6->icmp6_pointer = 0;  break;
    case 2  : icmp6->icmp6_pointer = 4;  break;
    case 8  : icmp6->icmp6_pointer = 7;  break;
    case 9  : icmp6->icmp6_pointer = 6;  break;
    case 12 : icmp6->icmp6_pointer = 8;  break;
    case 16 : icmp6->icmp6_pointer = 24; break;
    default : icmp6->icmp6_pointer = -1; break;
    }
     memcpy(ip6, (unsigned char *)ip, icmp6_length);
    icmp->icmp_cksum  = 0;
    icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.in6_addr(), &ip6_dst.in6_addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
  }
  break;

  default: ;

  }

  return q2;

}


void
ProtocolTranslator46::push(int, Packet *p)
{
    handle_ip4(p);
}


void
ProtocolTranslator46::handle_ip4(Packet *p)
{
  click_ip *ip = (click_ip *)p->data();

  IP6Address ip6a_src = IP6Address(IPAddress(ip->ip_src));
  IP6Address ip6a_dst = IP6Address(IPAddress(ip->ip_dst));

  unsigned char *start_of_p = (unsigned char *)(ip+1);
  Packet *q = 0;
  q=make_translate46(ip6a_src, ip6a_dst, ip, start_of_p);

  if (ip->ip_p == 1)
    {
      click_ip6 * ip6 = (click_ip6 *)q->data();
      unsigned char * icmp = (unsigned char *)(ip6+1);
      Packet *q2 = 0;
      q2 = make_icmp_translate46(ip6a_src, ip6a_dst, icmp, (q->length() - sizeof(click_ip6)));
      WritablePacket *q3 = Packet::make(sizeof(click_ip6)+q2->length());
      memset(q3->data(), '\0', q3->length());
      click_ip6 *start_of_q3 = (click_ip6 *)q3->data();
      memcpy(start_of_q3, q->data(), sizeof(click_ip6));
      unsigned char *start_of_icmp6 = (unsigned char *)(start_of_q3+1);
      memcpy(start_of_icmp6, q2->data(), q2->length());
      click_ip6 * ip62=(click_ip6 *)q3->data();
      ip62->ip6_plen = htons(q3->length()-sizeof(click_ip6));

      p->kill();
      q->kill();
      q2->kill();
      output(0).push(q3);
    }
  else
    {
      p->kill();
      output(0).push(q);
    }

}

CLICK_ENDDECLS
EXPORT_ELEMENT(ProtocolTranslator46)
