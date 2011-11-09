/*
 * ipprint.{cc,hh} -- element prints packet contents to system log
 * Max Poletto
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "printtime.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>

#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>

#if CLICK_USERLEVEL
# include <stdio.h>
#endif

IPPrintTime::IPPrintTime()
{
#if CLICK_USERLEVEL
  _outfile = 0;
#endif
}

IPPrintTime::~IPPrintTime()
{
}

int
IPPrintTime::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _bytes = 1500;
  String contents = "no";
  _label = "";
  _swap = false;
  bool print_id = false;
  bool print_time = false;
  bool print_paint = false;
  bool print_tos = false;
  bool print_ttl = false;
  bool print_len = false;
  String channel;

  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "label", &_label,
		  cpEnd) < 0)
    return -1;

  return 0;
}

int
IPPrintTime::initialize(ErrorHandler *errh)
{
  (void) errh;
  return 0;
}

void
IPPrintTime::uninitialize()
{
}

Packet *
IPPrintTime::simple_action(Packet *p)
{
  String s = "";

  if (!p->has_network_header()) {
    s = "(Not an IP packet)";
    return p;
  }

  const click_ip *iph = p->ip_header();
  IPAddress src(iph->ip_src.s_addr);
  IPAddress dst(iph->ip_dst.s_addr);
  unsigned ip_len = ntohs(iph->ip_len);
  //  unsigned char tos = iph->ip_tos;

  StringAccum sa;
  if (_label)
    sa << _label << ": ";

  switch (iph->ip_p) {

  case IP_PROTO_TCP: {
    const click_tcp *tcph = p->tcp_header();
    unsigned short srcp = ntohs(tcph->th_sport);
    unsigned short dstp = ntohs(tcph->th_dport);
    unsigned seq = ntohl(tcph->th_seq);
    unsigned ack = ntohl(tcph->th_ack);
    unsigned win = ntohs(tcph->th_win);
    unsigned seqlen = ip_len - (iph->ip_hl << 2) - (tcph->th_off << 2);
    int ackp = tcph->th_flags & TH_ACK;

    sa << src << '.' << srcp << " > " << dst << '.' << dstp << ": ";
    if (tcph->th_flags & TH_SYN)
      sa << 'S', seqlen++;
    if (tcph->th_flags & TH_FIN)
      sa << 'F', seqlen++;
    if (tcph->th_flags & TH_RST)
      sa << 'R';
    if (tcph->th_flags & TH_PUSH)
      sa << 'P';
    if (!(tcph->th_flags & (TH_SYN | TH_FIN | TH_RST | TH_PUSH)))
      sa << '.';

    break;
  }

  case IP_PROTO_UDP: {
    const click_udp *udph = p->udp_header();
    unsigned short srcp = ntohs(udph->uh_sport);
    unsigned short dstp = ntohs(udph->uh_dport);
    unsigned len = ntohs(udph->uh_ulen);
    sa << src << '.' << srcp << " > " << dst << '.' << dstp << ": udp " << len;
    break;
  }

  default: {
    sa << src << " > " << dst << ": ip protocol " << (int)iph->ip_p;
    break;
  }

  }


  _errh->message("%s", sa.c_str());
  return p;
}

EXPORT_ELEMENT(IPPrintTime)
