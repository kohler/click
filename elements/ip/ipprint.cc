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
#include "ipprint.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>

#include <click/click_ip.h>
#include <click/click_icmp.h>
#include <click/click_tcp.h>
#include <click/click_udp.h>

#if CLICK_USERLEVEL
# include <stdio.h>
#endif

IPPrint::IPPrint()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
#if CLICK_USERLEVEL
  _outfile = 0;
#endif
}

IPPrint::~IPPrint()
{
  MOD_DEC_USE_COUNT;
}

IPPrint *
IPPrint::clone() const
{
  return new IPPrint;
}

int
IPPrint::configure(Vector<String> &conf, ErrorHandler *errh)
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
		  cpKeywords,
		  "CONTENTS", cpWord, "print packet contents (no/hex/ascii)", &contents,
		  "NBYTES", cpInteger, "max data bytes to print", &_bytes,
		  "ID", cpBool, "print IP ID?", &print_id,
		  "TIMESTAMP", cpBool, "print packet timestamps?", &print_time,
		  "PAINT", cpBool, "print paint?", &print_paint,
		  "TOS", cpBool, "print IP TOS?", &print_tos,
		  "TTL", cpBool, "print IP TTL?", &print_ttl,
		  "SWAP", cpBool, "swap ICMP values when printing?", &_swap,
		  "LENGTH", cpBool, "print IP length?", &print_len,
#if CLICK_USERLEVEL
		  "OUTFILE", cpFilename, "output filename", &_outfilename,
#endif
		  "CHANNEL", cpWord, "output channel", &channel,
		  cpEnd) < 0)
    return -1;

  contents = contents.upper();
  if (contents == "NO" || contents == "FALSE")
    _contents = 0;
  else if (contents == "YES" || contents == "TRUE" || contents == "HEX")
    _contents = 1;
  else if (contents == "ASCII")
    _contents = 2;
  else
    return errh->error("bad contents value `%s'; should be `false', `hex', or `ascii'", contents.cc());

  _print_id = print_id;
  _print_timestamp = print_time;
  _print_paint = print_paint;
  _print_tos = print_tos;
  _print_ttl = print_ttl;
  _print_len = print_len;
  _errh = router()->chatter_channel(channel);
  return 0;
}

int
IPPrint::initialize(ErrorHandler *errh)
{
#if CLICK_USERLEVEL
  if (_outfilename) {
    _outfile = fopen(_outfilename, "wb");
    if (!_outfile)
      return errh->error("%s: %s", _outfilename.cc(), strerror(errno));
  }
#else
  (void) errh;
#endif
  return 0;
}

void
IPPrint::uninitialize()
{
#if CLICK_USERLEVEL
  if (_outfile) {
    fclose(_outfile);
    _outfile = 0;
  }
#endif
}

Packet *
IPPrint::simple_action(Packet *p)
{
  String s = "";
  const click_ip *iph = p->ip_header();

  if (!iph) {
    s = "(Not an IP packet)";
    return p;
  }

  IPAddress src(iph->ip_src.s_addr);
  IPAddress dst(iph->ip_dst.s_addr);
  unsigned ip_len = ntohs(iph->ip_len);
  //  unsigned char tos = iph->ip_tos;

  StringAccum sa;
  if (_label)
    sa << _label << ": ";

  if (_print_timestamp)
    sa << p->timestamp_anno() << ": ";
  
  if (_print_id)
    sa << "id " << ntohs(iph->ip_id) << ' ';
  if (_print_paint)
    sa << "paint " << (int)PAINT_ANNO(p) << ' ';
  if (_print_ttl)
    sa << "ttl " << (int)iph->ip_ttl << ' ';
  if (_print_tos)
    sa << "tos " << (int)iph->ip_tos << ' ';
  if (_print_len)
    sa << "length " << ntohs(iph->ip_len) << ' ';

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
    
    sa << ' ' << seq << ':' << (seq + seqlen)
       << '(' << seqlen << ',' << p->length() << ',' << ip_len << ')';
    if (ackp)
      sa << " ack " << ack;
    sa << " win " << win;
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

#define swapit(x) (_swap ? ((((x) & 0xff) << 8) | ((x) >> 8)) : (x))
  
  case IP_PROTO_ICMP: {
    sa << src << " > " << dst << ": icmp";
    const icmp_generic *icmph = reinterpret_cast<const icmp_generic *>(p->transport_header());
    if (icmph->icmp_type == ICMP_ECHO_REPLY) {
      const icmp_sequenced *seqh = reinterpret_cast<const icmp_sequenced *>(icmph);
      sa << ": echo reply (" << swapit(seqh->identifier) << ", " << swapit(seqh->sequence) << ")";
    } else if (icmph->icmp_type == ICMP_ECHO) {
      const icmp_sequenced *seqh = reinterpret_cast<const icmp_sequenced *>(icmph);
      sa << ": echo request (" << swapit(seqh->identifier) << ", " << swapit(seqh->sequence) << ")";
    } else
      sa << ": type " << (int)icmph->icmp_type;
    break;
  }
  
  default: {
    sa << src << " > " << dst << ": ip protocol " << (int)iph->ip_p;
    break;
  }
  
  }

  const unsigned char *data = p->data();
  if (_contents > 0) {
    int amt = 3*_bytes + (_bytes/4+1) + 3*(_bytes/24+1) + 1;

    sa << "\n  ";
    char *buf = sa.reserve(amt);
    char *orig_buf = buf;

    if (buf && _contents == 1) {
      for (unsigned i = 0; i < _bytes && i < p->length(); i++) {
	sprintf(buf, "%02x", data[i] & 0xff);
	buf += 2;
	if ((i % 24) == 23) {
	  *buf++ = '\n'; *buf++ = ' '; *buf++ = ' ';
	} else if ((i % 4) == 3)
	  *buf++ = ' ';
      }
    } else if (buf && _contents == 2) {
      for (unsigned i = 0; i < _bytes && i < p->length(); i++) {
	if (data[i] < 32 || data[i] > 126)
	  *buf++ = '.';
	else
	  *buf++ = data[i];
	if ((i % 48) == 47) {
	  *buf++ = '\n'; *buf++ = ' '; *buf++ = ' ';
	} else if ((i % 8) == 7)
	  *buf++ = ' ';
      }
    }

    if (orig_buf) {
      assert(buf <= orig_buf + amt);
      sa.forward(buf - orig_buf);
    }
  }

#if CLICK_USERLEVEL
  if (_outfile) {
    sa << '\n';
    fwrite(sa.data(), 1, sa.length(), _outfile);
  } else
#endif
  {
    _errh->message("%s", sa.cc());
  }

  return p;
}

EXPORT_ELEMENT(IPPrint)
