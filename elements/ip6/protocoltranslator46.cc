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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "protocoltranslator46.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/click_ip.h>
#include <click/click_ip6.h>
#include <click/click_icmp.h>
#include <click/click_icmp6.h>
#include <click/click_tcp.h>
#include <click/click_udp.h>


ProtocolTranslator46::ProtocolTranslator46()
{
  add_input();  /*IPv4 packets */
  add_output(); /* IPv6 packets */
}


ProtocolTranslator46::~ProtocolTranslator46()
{
}


int
ProtocolTranslator46::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  //can add configuration later on to deal with other protocols' translation
  if (conf.size()==0) 
    {
      //no argument, default as 4_to_6 protocol translator
    }
  else
    {
     
    }
  int before = errh->nerrors();
  return (before ==errh->nerrors() ? 0: -1);
 
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
  ip6->ip6_v=6;
  ip6->ip6_pri=0;
  for (int i=0; i<3; i++)
    ip6->ip6_flow[i]=0;
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
  case (ICMP_ECHO_REPLY):  {//icmp_type ==0 
    icmp_echo *icmp = (icmp_echo *)a;
    ip = (click_ip *)(icmp+1); 
    icmp6_length = payload_length-sizeof(icmp_echo)+sizeof(icmp6_echo);
    q2=Packet::make(icmp6_length);
    memset(q2->data(), '\0', q2->length());
    icmp6_echo *icmp6 = (icmp6_echo *)q2->data();
    ip6=(unsigned char *)(icmp6+1);
      
    if (icmp_type == ICMP_ECHO ) { // icmp_type ==8
      icmp6->icmp6_type = ICMP6_ECHO_REQUEST;  // icmp6_type =128  
    }
    else if (icmp_type == ICMP_ECHO_REPLY ) { // icmp_type ==0
      icmp6->icmp6_type = ICMP6_ECHO_REPLY;              // icmp6_type = 129   
    }
    icmp6->identifier = icmp->identifier;
    icmp6->sequence = icmp->sequence;
    memcpy(ip6, (unsigned char *)ip, icmp6_length);
    icmp->icmp_cksum  = 0;
  icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.addr(), &ip6_dst.addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
  
  }
  break; 

  case (ICMP_DST_UNREACHABLE):  { //icmp_type ==3
    icmp_unreach *icmp = (icmp_unreach *)a;
    ip = (click_ip *)(icmp+1);
    
    if (icmp_code == 2 || icmp_code ==4) {
      switch (icmp_code) {
      case 2: {
	icmp6_length = payload_length-sizeof(icmp_unreach)+sizeof(icmp6_param);
	q2=Packet::make(icmp6_length);
	memset(q2->data(), '\0', q2->length());
	icmp6_param *icmp6 = (icmp6_param *)q2->data();
	ip6=(unsigned char *)(icmp6+1);
	icmp6->icmp6_type = ICMP6_PARAMETER_PROBLEM; //icmp6_type = 4
	icmp6->icmp6_code = 1;
	icmp6->pointer = 6;
	memcpy(ip6, (unsigned char *)ip, icmp6_length);
	icmp->icmp_cksum  = 0;
	icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.addr(), &ip6_dst.addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
      }
      break;
	    
      case 4: {
	icmp6_length = payload_length-sizeof(icmp_unreach)+sizeof(icmp6_pkt_toobig);
	q2=Packet::make(icmp6_length);
	memset(q2->data(), '\0', q2->length());
	icmp6_pkt_toobig *icmp6 = (icmp6_pkt_toobig *)q2->data();
	ip6=(unsigned char *)(icmp6+1);
	icmp6->icmp6_type = ICMP6_PKT_TOOBIG; //icmp6_type = 2
	icmp6->icmp6_code = 0;

	//adjust the mtu field for the difference between the ipv4 and ipv6 header size

	memcpy(ip6, (unsigned char *)ip, icmp6_length);
	icmp->icmp_cksum  = 0;
	icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.addr(), &ip6_dst.addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
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
      default:  {
	icmp6_length = payload_length-sizeof(icmp_unreach)+sizeof(icmp6_dst_unreach);
	q2=Packet::make(icmp6_length);
	memset(q2->data(), '\0', q2->length());
	icmp6_dst_unreach *icmp6 = (icmp6_dst_unreach *)q2->data();
	ip6=(unsigned char *)(icmp6+1);
	icmp6->icmp6_type = ICMP6_DST_UNREACHABLE;
	icmp6->icmp6_code = icmp6_code;
	memcpy(ip6, (unsigned char *)ip, icmp6_length);
	icmp->icmp_cksum  = 0;
	icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.addr(), &ip6_dst.addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
      }
      break;
      }
    }
  }
  break; 
     
  case (ICMP_TYPE_TIME_EXCEEDED) : { //icmp ==11
    icmp_exceeded *icmp = (icmp_exceeded *)a;
    ip = (click_ip *)(icmp+1);
    icmp6_length = payload_length-sizeof(icmp_exceeded)+sizeof(icmp6_time_exceeded);
    q2=Packet::make(icmp6_length);
    memset(q2->data(), '\0', q2->length());
    icmp6_echo *icmp6 = (icmp6_echo *)q2->data();
    ip6=(unsigned char *)(icmp6+1);
    icmp6->icmp6_type=ICMP6_TYPE_TIME_EXCEEDED;
    icmp6->icmp6_code = icmp_code;
    memcpy(ip6, (unsigned char *)ip, icmp6_length);
    icmp->icmp_cksum  = 0;
    icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.addr(), &ip6_dst.addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
  }
  break;

  case (ICMP_PARAMETER_PROBLEM): { //icmp==12
    icmp_param *icmp = (icmp_param *)a;
    ip = (click_ip *)(icmp+1);
    icmp6_length = payload_length-sizeof(icmp_param)+sizeof(icmp6_param);
    q2=Packet::make(icmp6_length);
    memset(q2->data(), '\0', q2->length());
    icmp6_param *icmp6 = (icmp6_param *)q2->data();
    ip6=(unsigned char *)(icmp6+1);
    icmp6->icmp6_type=ICMP6_PARAMETER_PROBLEM;
	  
    switch (icmp_pointer) {
    case 0  : icmp6->pointer = 0;  break;
    case 2  : icmp6->pointer = 4;  break;
    case 8  : icmp6->pointer = 7;  break;
    case 9  : icmp6->pointer = 6;  break;
    case 12 : icmp6->pointer = 8;  break;
    case 16 : icmp6->pointer = 24; break;
    default : icmp6->pointer = -1; break;
    }
     memcpy(ip6, (unsigned char *)ip, icmp6_length);
    icmp->icmp_cksum  = 0;
    icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_src.addr(), &ip6_dst.addr(), htons(icmp6_length), 0x3a, 0, (unsigned char *)icmp6, htons(icmp6_length)));
  }
  break;

  default: ;
  
  }

  return q2;
   
}


void
ProtocolTranslator46::push(int port, Packet *p)
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

EXPORT_ELEMENT(ProtocolTranslator46)


