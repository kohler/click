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
TCPIPSend::configure(const Vector<String> &, ErrorHandler *)
{
  return 0;
}

int
TCPIPSend::send_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  TCPIPSend* me = (TCPIPSend *) e;

  unsigned int saddr, daddr;
  unsigned short sport, dport;
  unsigned char bits;
  if(cp_va_space_parse(conf, me, errh,
       cpIPAddress, "source address", &saddr,
       cpInteger, "source port", &sport,
       cpIPAddress, "destination address", &daddr,
       cpInteger, "destinatin port", &dport,
       cpByte, "bits", &bits,
       0) < 0)
    return -1;

  Packet *p = me->make_packet(saddr, daddr, sport, dport, bits);
  me->output(0).push(p);
  return 0;
}



Packet *
TCPIPSend::make_packet(unsigned int saddr, unsigned int daddr,
                       unsigned short sport, unsigned short dport,
                       unsigned char bits)
{
  struct click_ip *ip;
  struct click_tcp *tcp;
  Packet *q = Packet::make(sizeof(*ip) + sizeof(*tcp));
  if (q == 0) {
    click_chatter("in TCPIPSend: cannot make packet!");
    assert(0);
  } 
  memset(q->data(), '\0', q->length());
  ip = (struct click_ip *) q->data();
  tcp = (struct click_tcp *) (ip + 1);

  // TCP fields
  sport = htons(sport);
  dport = htons(dport);
  memcpy((void *) &(tcp->th_sport), (void *) &sport, sizeof(sport));
  memcpy((void *) &(tcp->th_dport), (void *) &dport, sizeof(dport));
  tcp->th_seq = 0;
  tcp->th_ack = 0;
  tcp->th_off = 5;
  tcp->th_flags = bits;
  tcp->th_win = htons(0);
  tcp->th_sum = htons(0);
  tcp->th_urp = htons(0);
#if 1 || !defined(__KERNEL__)
  tcp->th_sum = in_cksum((unsigned char *)tcp, sizeof(click_tcp));
#else
  tcp->th_sum = ip_fast_csum((unsigned char *)tcp, sizeof(click_tcp) >> 2);
#endif

  // IP fields
  ip->ip_v = IPVERSION;
  ip->ip_hl = 5;
  ip->ip_tos = 0;
  ip->ip_len = htons(q->length());
  ip->ip_id = htons(0);
  ip->ip_off = htons(0);
  ip->ip_ttl = 255;
  ip->ip_p = IP_PROTO_TCP;
  ip->ip_sum = 0;
  memcpy((void *) &(ip->ip_src), (void *) &saddr, sizeof(saddr));
  memcpy((void *) &(ip->ip_dst), (void *) &daddr, sizeof(daddr));
#ifndef __KERNEL__
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(click_ip));
#else
  ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#endif

  return q;
}


void
TCPIPSend::add_handlers()
{
  add_write_handler("send", send_write_handler, 0);
}




EXPORT_ELEMENT(TCPIPSend)
