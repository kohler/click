/*
 * tcpipsend.{cc,hh} -- sends TCP/IP packets
 * Thomer M. Gil
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
#include "tcpipsend.hh"
#include "click_ip.h"
#include "click_tcp.h"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

TCPIPSend::TCPIPSend()
{
  add_output();
}

TCPIPSend::~TCPIPSend()
{
}

TCPIPSend *
TCPIPSend::clone() const
{
  return new TCPIPSend;
}

int
TCPIPSend::send_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  TCPIPSend* me = (TCPIPSend *) e;

  unsigned int saddr, daddr;
  unsigned short sport, dport;
  unsigned char bits;
  unsigned seqn, ackn;
  if(cp_va_space_parse(conf, me, errh,
       cpIPAddress, "source address", &saddr,
       cpInteger, "source port", &sport,
       cpIPAddress, "destination address", &daddr,
       cpInteger, "destinatin port", &dport,
       cpUnsigned, "seq number", &seqn,
       cpUnsigned, "ack number", &ackn,
       cpByte, "bits", &bits,
       0) < 0)
    return -1;

  Packet *p = me->make_packet(saddr, daddr, sport, dport, seqn, ackn, bits);
  me->output(0).push(p);
  return 0;
}



Packet *
TCPIPSend::make_packet(unsigned int saddr, unsigned int daddr,
                       unsigned short sport, unsigned short dport,
		       unsigned int seqn, unsigned int ackn,
                       unsigned char bits)
{
  struct click_ip *ip;
  struct click_tcp *tcp;
  WritablePacket *q = Packet::make(sizeof(*ip) + sizeof(*tcp));
  if (q == 0) {
    click_chatter("in TCPIPSend: cannot make packet!");
    assert(0);
  } 
  memset(q->data(), '\0', q->length());
  ip = (struct click_ip *) q->data();
  tcp = (struct click_tcp *) (ip + 1);
  
  // IP fields
  ip->ip_v = 4;
  ip->ip_hl = 5;
  ip->ip_tos = 0;
  ip->ip_len = htons(q->length());
  ip->ip_id = htons(0);
  ip->ip_off = htons(IP_DF);
  ip->ip_ttl = 255;
  ip->ip_p = IP_PROTO_TCP;
  ip->ip_sum = 0;
  memcpy((void *) &(ip->ip_src), (void *) &saddr, sizeof(saddr));
  memcpy((void *) &(ip->ip_dst), (void *) &daddr, sizeof(daddr));
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));

  // TCP fields
  sport = htons(sport);
  dport = htons(dport);
  memcpy((void *) &(tcp->th_sport), (void *) &sport, sizeof(sport));
  memcpy((void *) &(tcp->th_dport), (void *) &dport, sizeof(dport));
  tcp->th_seq = htonl(seqn);
  tcp->th_ack = htonl(ackn);
  tcp->th_off = 5;
  tcp->th_flags = bits;
  tcp->th_win = htons(32120);
  tcp->th_sum = htons(0);
  tcp->th_urp = htons(0);

  // now calculate tcp header cksum
  unsigned csum =
    ~in_cksum((unsigned char *)tcp, sizeof(click_tcp)) & 0xFFFF;
#ifdef __KERNEL__
  tcp->th_sum = csum_tcpudp_magic
    (ip->ip_src.s_addr, ip->ip_dst.s_addr, sizeof(click_tcp),
     IP_PROTO_TCP, csum);
#else
  unsigned short *words = (unsigned short *)&ip->ip_src;
  csum += words[0];
  csum += words[1];
  csum += words[2];
  csum += words[3];
  csum += htons(IP_PROTO_TCP);
  csum += htons(sizeof(click_tcp));
  while (csum >> 16)
    csum = (csum & 0xFFFF) + (csum >> 16);
  tcp->th_sum = ~csum & 0xFFFF;
#endif

  return q;
}


void
TCPIPSend::add_handlers()
{
  add_write_handler("send", send_write_handler, 0);
}




EXPORT_ELEMENT(TCPIPSend)
