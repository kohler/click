/*
 * protocoltranslator64{cc,hh} -- element that translates an IPv6 packet to
 * an IPv4 packet.
 * When translating an IPv6 packet to IPv4 packet, assuming the addresses has
 * already been IPv4-mapped IPv6 addresses (e.g.,::ffff:18.26.4.17), the IPv4
 * addresses will be the lowest 32 bits of IPv6 addresses.
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
#include "protocoltranslator64.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/ip6.h>
#include <clicknet/icmp.h>
#include <clicknet/icmp6.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
CLICK_DECLS

ProtocolTranslator64::ProtocolTranslator64()
{
}


ProtocolTranslator64::~ProtocolTranslator64()
{
}


//make the ipv6->ipv4 translation of the packet according to SIIT (RFC 2765)
Packet *
ProtocolTranslator64::make_translate64(IPAddress src,
				     IPAddress dst,
				     click_ip6 * ip6,
				     unsigned char *a)
{
  click_ip *ip;
  click_tcp *tcp;
  click_udp *udp;
  WritablePacket *q = Packet::make(sizeof(*ip) + ntohs(ip6->ip6_plen));

  if (q==0) {
    click_chatter("can not make packet!");
    assert(0);
  }

  memset(q->data(), '\0', q->length());
  ip = (click_ip *)q->data();
  tcp = (click_tcp *)(ip+1);
  udp = (click_udp *)(ip+1);

  //set ipv4 header
  ip->ip_v = 4;
  ip->ip_hl =5;
  ip->ip_tos =0;
  ip->ip_len = htons(sizeof(*ip) + ntohs(ip6->ip6_plen));

  ip->ip_id = htons(0);
  //need to change
  //ip->ip_id[0]=ip6->ip_flow[1];
  //ip->ip_id[1]=ip6->ip_flow[2];

  //set Don't Fragment flag to true, all other flags to false,
  //set fragement offset: 0
  ip->ip_off = htons(IP_DF);
  //need to deal with fragmentation later

  //we do not change the ttl since the packet has to go through v4 routing table
  ip->ip_ttl = ip6->ip6_hlim;


  //set the src and dst address
  ip->ip_src = src.in_addr();
  ip->ip_dst = dst.in_addr();

  //copy the actual payload of packet
  memcpy((unsigned char *)tcp, a, ntohs(ip6->ip6_plen));
  //set the tcp header checksum
  //The tcp checksum for ipv4 packet is include the tcp packet, and the 96 bits
  //TCP pseudoheader, which consists of Source Address, Destination Address,
  //1 byte zero, 1 byte PTCL, 2 byte TCP length.

  if (ip6->ip6_nxt == 6) //TCP
    {
      ip->ip_p = ip6->ip6_nxt;

      //set the ip header checksum
      ip->ip_sum = 0;
      tcp->th_sum = 0;

      uint16_t tlen = ntohs(ip6->ip6_plen);
      uint16_t csum = click_in_cksum((unsigned char *) tcp, tlen);
      tcp->th_sum = click_in_cksum_pseudohdr(csum, ip, tlen);
      ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));

    }

  else if (ip6->ip6_nxt ==17) //UDP
    {
      ip->ip_p = ip6->ip6_nxt;

      //set the ip header checksum
      ip->ip_sum=0;
      udp->uh_sum = 0;

      uint16_t tlen = ntohs(ip6->ip6_plen);
      uint16_t csum = click_in_cksum((unsigned char *) udp, tlen);
      udp->uh_sum = click_in_cksum_pseudohdr(csum, ip, tlen);
      ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));

    }

  else if (ip6->ip6_nxt== 58)
    {
      ip->ip_p = 1;
      //icmp 4->6 translation is dealt by caller
    }

  else
    {
      // will deal with other protocols later
    }
  return q;

}


Packet *
ProtocolTranslator64::make_icmp_translate64(unsigned char *a,
			      unsigned char payload_length)
{

  click_ip6 *ip6=0;
  unsigned char *ip=0;
  unsigned char icmp6_type= a[0];
  unsigned char icmp6_code= a[1];
  unsigned char icmp_length;
  WritablePacket *q2 = 0;

  // set type, code, length , and checksum of ICMP header
  switch (icmp6_type) {
   case ICMP6_ECHO: ;
   case ICMP6_ECHOREPLY : {
      icmp_length = payload_length -sizeof(click_icmp6_echo) + sizeof(click_icmp_echo);
      click_icmp6_echo *icmp6 = (click_icmp6_echo *)a;
      ip6 = (click_ip6 *)(icmp6+1);

      q2 = Packet::make(icmp_length);
      memset(q2->data(), '\0', q2->length());
      click_icmp_echo *icmp = (click_icmp_echo *)q2->data();
      ip = (unsigned char *)(icmp+1);

      if (icmp6_type == ICMP6_ECHO ) { // icmp6_type ==128
	icmp->icmp_type = ICMP_ECHO;                 // icmp_type =8
      }
      else if (icmp6_type == ICMP6_ECHOREPLY ) { // icmp6_type ==129
	icmp->icmp_type = ICMP_ECHOREPLY;              // icmp_type = 0
      }

      icmp->icmp_identifier = (icmp6->icmp6_identifier);
      icmp->icmp_sequence = (icmp6->icmp6_sequence);
      memcpy(ip, ip6, (payload_length - sizeof(click_icmp6_echo)));
      icmp->icmp_cksum = click_in_cksum((unsigned char *)icmp, icmp_length);

    }
  break;

  case  ICMP6_UNREACH : {
      // icmp6_type ==11
      icmp_length = payload_length-sizeof(click_icmp6_unreach)+sizeof(click_icmp_unreach);
      click_icmp6_unreach *icmp6 = (click_icmp6_unreach *)a;
      ip6 = (click_ip6 *)(icmp6+1);
      q2 = Packet::make(icmp_length);
      memset(q2->data(), '\0', q2->length());
      click_icmp_unreach *icmp = (click_icmp_unreach *)q2->data();
      ip = (unsigned char *)(icmp+1);
      icmp->icmp_type = ICMP_UNREACH;             // icmp_type =3

      switch (icmp6_code) {
      case 0: icmp->icmp_code =0;  break;
      case 1: icmp->icmp_code =10; break;
      case 2: icmp->icmp_code =5;  break;
      case 3: icmp->icmp_code =1;  break;
      case 4: icmp->icmp_code =3;  break;
      default: ; break;
      }
      memcpy(ip, ip6, (payload_length - sizeof(click_icmp6_unreach)));
      icmp->icmp_cksum = click_in_cksum((unsigned char *)icmp, icmp_length);

    }
  break;

  case ICMP6_PKTTOOBIG : {
      icmp_length = payload_length-sizeof(click_icmp6_pkttoobig)+sizeof(click_icmp_unreach);
      click_icmp6_pkttoobig *icmp6 = (click_icmp6_pkttoobig*)a;
      ip6 = (click_ip6 *)(icmp6+1);
      q2 = Packet::make(icmp_length);
      memset(q2->data(), '\0', q2->length());
      click_icmp_unreach *icmp = (click_icmp_unreach *)q2->data();
      ip = (unsigned char *)(icmp+1);
      icmp->icmp_type = ICMP_UNREACH; //icmp_type = 3
      icmp->icmp_code = 4;
      memcpy(ip, ip6, (payload_length - sizeof(click_icmp6_pkttoobig)));
      icmp->icmp_cksum = click_in_cksum((unsigned char *)icmp, icmp_length);
    }
  break;

  case ICMP6_TIMXCEED : {
      icmp_length = payload_length-sizeof(click_icmp6_timxceed)+sizeof(click_icmp_timxceed);
      click_icmp6_timxceed *icmp6 = (click_icmp6_timxceed *)a;
      ip6 = (click_ip6 *)(icmp6+1);
      q2 = Packet::make(icmp_length);
      memset(q2->data(), '\0', q2->length());
      click_icmp_timxceed *icmp = (click_icmp_timxceed *)q2->data();
      ip = (unsigned char *)(icmp+1);
      icmp->icmp_type =ICMP_TIMXCEED;            //icmp_type = 11
      icmp->icmp_code = icmp6_code;
      memcpy(ip, ip6, (payload_length - sizeof(click_icmp6_timxceed)));
      icmp->icmp_cksum = click_in_cksum((unsigned char *)icmp, icmp_length);
    }
  break;

  case ICMP6_PARAMPROB : { // icmp6_type == 4

      click_icmp6_paramprob *icmp6 = (click_icmp6_paramprob *)a;
      ip6=(click_ip6 *)(icmp6+1);
      if (icmp6_code ==2 || icmp6_code ==0)
	{
	  icmp_length = payload_length-sizeof(click_icmp6_paramprob)+sizeof(click_icmp_paramprob);
	  q2 = Packet::make(icmp_length);
	  memset(q2->data(), '\0', q2->length());
	  click_icmp_paramprob *icmp = (click_icmp_paramprob *)q2->data();
	  ip = (unsigned char *)(icmp +1);
	  icmp->icmp_type = ICMP_PARAMPROB;         // icmp_type = 12
	  icmp->icmp_code = 0;
	  if (icmp6_code ==2) {
	    icmp->icmp_pointer = -1;
	  }
	  else if(icmp6_code ==0)
	    {
	      switch (ntohl(icmp6->icmp6_pointer)) {
	      case 0 : icmp->icmp_pointer = 0;  break;
	      case 4 : icmp->icmp_pointer = 2;  break;
	      case 7 : icmp->icmp_pointer = 8;  break;
	      case 6 : icmp->icmp_pointer = 9;  break;
	      case 8 : icmp->icmp_pointer = 12; break;
	      case 24: icmp->icmp_pointer = -1; break;
	      default: ; break;
	      }

	    }
	  memcpy(ip, ip6, (payload_length - sizeof(click_icmp6_paramprob)));
	  icmp->icmp_cksum = click_in_cksum((unsigned char *)icmp, icmp_length);
	}

      else if (icmp6_code ==1)
	{
	  icmp_length = payload_length-sizeof(click_icmp6_paramprob)+sizeof(click_icmp_unreach);
	  q2 = Packet::make(icmp_length);
	  memset(q2->data(), '\0', q2->length());
	  click_icmp_unreach *icmp = (click_icmp_unreach *)q2->data();

	  ip = (unsigned char *)(icmp +1);
	  icmp->icmp_type = ICMP_UNREACH; // icmp_type = 3
	  icmp->icmp_code = 2;
	  memcpy(ip, ip6, (payload_length - sizeof(click_icmp6_paramprob)));
	  icmp->icmp_cksum = click_in_cksum((unsigned char *)icmp, icmp_length);
	}
  }
  break;

  default: ; break;
  }

  return q2;
}




void
ProtocolTranslator64::push(int, Packet *p)
{
  handle_ip6(p);
}


void
ProtocolTranslator64::handle_ip6(Packet *p)
{
  click_ip6 *ip6 = (click_ip6 *) p->data();
  IP6Address ip6_dst = IP6Address(ip6->ip6_dst);
  IP6Address ip6_src = IP6Address(ip6->ip6_src);

  IPAddress ipa_dst = ip6_dst.ip4_address(), ipa_src = ip6_src.ip4_address();
  if (ipa_dst && ipa_src)
    {

       //translate protocol according to SIIT
       unsigned char * start_of_p= (unsigned char *)(ip6+1);
       Packet *q = 0;
       q = make_translate64(ipa_src, ipa_dst, ip6, start_of_p);

       //see if it is an icmp6 packet, if it is, translate the icmp6 packet
       if (ip6->ip6_nxt == 0x3a)
	 {
	   click_ip * ip4= (click_ip *)q->data();
	   unsigned char * icmp6 = (unsigned char *)(ip4+1);

	   Packet *q2 = 0;
	   q2 = make_icmp_translate64(icmp6, (q->length()-sizeof(click_ip)));
	   WritablePacket *q3=Packet::make(sizeof(click_ip)+q2->length());
	   memset(q3->data(), '\0', q3->length());
	   click_ip *ip = (click_ip *)q3->data();
	   memcpy(ip, q->data(), sizeof(click_ip));
	   unsigned char *start_of_icmp = (unsigned char *)(ip+1);
	   memcpy(start_of_icmp, q2->data(), q2->length());
	   ip->ip_len = htons(q3->length());
	   ip->ip_sum=0;
	   ip->ip_sum = click_in_cksum((unsigned char *)ip, q3->length());

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

  else
    {
      p->kill();
      return;
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ProtocolTranslator64)
