/*
 * ipprint.{cc,hh} -- element prints packet contents to system log
 * Max Poletto
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "ipprint.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>

#include <click/click_ip.h>
#include <click/click_tcp.h>
#include <click/click_udp.h>

IPPrint::IPPrint()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _buf = 0;
}

IPPrint::~IPPrint()
{
  MOD_DEC_USE_COUNT;
  delete[] _buf;
}

IPPrint *
IPPrint::clone() const
{
  return new IPPrint;
}

int
IPPrint::configure(const Vector<String> &conf, ErrorHandler* errh)
{
  _bytes = 1500;
  String contents = "no";
  _label = "";
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "label", &_label,
		  cpWord, "print packet contents (no/hex/ascii)", &contents,
		  cpInteger, "number of bytes to dump", &_bytes,
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
    return errh->error("bad contents value `%s'; should be NO, HEX, or ASCII", contents.cc());
  
  delete[] _buf;
  _buf = 0;

  if (_contents) {
    _buf = new char[3*_bytes+(_bytes/4+1)+3*(_bytes/24+1)+1];
    if (!_buf)
      return errh->error("out of memory");
  }
  
  return 0;
}

void
IPPrint::uninitialize()
{
  delete[] _buf;
  _buf = 0;
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
  
  switch (iph->ip_p) {
    
  case IP_PROTO_TCP: {
    const click_tcp *tcph =
      reinterpret_cast<const click_tcp *>(p->transport_header());
    unsigned short srcp = ntohs(tcph->th_sport);
    unsigned short dstp = ntohs(tcph->th_dport);
    unsigned seq = ntohl(tcph->th_seq);
    unsigned ack = ntohl(tcph->th_ack);
    unsigned win = ntohs(tcph->th_win);
    unsigned seqlen = ip_len - (iph->ip_hl << 2) - (tcph->th_off << 2);
    int ackp = tcph->th_flags & TH_ACK;
    String flag = "";
    if (tcph->th_flags & TH_SYN)
      flag += "S", seqlen++;
    if (tcph->th_flags & TH_FIN)
      flag += "F", seqlen++;
    if (tcph->th_flags & TH_RST)
      flag += "R";
    if (tcph->th_flags & TH_PUSH)
      flag += "P";
    if (flag.length() == 0)
      flag = ".";
    sa << src.s() << '.' << srcp << " > " << dst.s() << '.' << dstp
       << ": " << flag << ' ' << seq << ':' << (seq + seqlen)
       << '(' << seqlen << ',' << p->length() << ',' << ip_len << ')';
    if (ackp)
      sa << " ack " << ack;
    sa << " win " << win;
    break;
  }
  
  case IP_PROTO_UDP: {
    const click_udp *udph =
      reinterpret_cast<const click_udp *>(p->transport_header());
    unsigned short srcp = ntohs(udph->uh_sport);
    unsigned short dstp = ntohs(udph->uh_dport);
    unsigned len = ntohs(udph->uh_ulen);
    sa << src.s() << '.' << srcp << " > "
       << dst.s() << '.' << dstp << ": udp " << len;
    break;
  }
  
  case IP_PROTO_ICMP: {
    sa << src.s() << " > " << dst.s() << ": icmp";
    break;
  }
  
  default: {
    sa << src.s() << " > " << dst.s() << ": ip protocol " << iph->ip_p;
    break;
  }
  
  }

  sa << '\0';
  click_chatter("%s", sa.data());

  const unsigned char *data = p->data();
  if (_contents == 1) {
    int pos = 0;  
    for (unsigned i = 0; i < _bytes && i < p->length(); i++) {
      sprintf(_buf+pos, "%02x", data[i] & 0xff);
      pos += 2;
      if ((i % 24) == 23) {
	_buf[pos++] = '\n'; _buf[pos++] = ' ';	_buf[pos++] = ' ';
      } else if ((i % 4) == 3)
	_buf[pos++] = ' ';
    }
    _buf[pos++] = '\0';
#ifdef __KERNEL__
    printk("  %s\n", _buf);
#else
    fprintf(stderr, "  %s\n", _buf);
#endif
  } else if (_contents == 2) {
    int pos = 0;  
    for (unsigned i = 0; i < _bytes && i < p->length(); i++) {
      if (data[i] < 32 || data[i] > 126)
	_buf[pos++] = '.';
      else
	_buf[pos++] = data[i];
      if ((i % 48) == 47) {
	_buf[pos++] = '\n'; _buf[pos++] = ' ';	_buf[pos++] = ' ';
      } else if ((i % 8) == 7)
	_buf[pos++] = ' ';
    }
    _buf[pos++] = '\0';
#ifdef __KERNEL__
    printk("  %s\n", _buf);
#else
    fprintf(stderr, "  %s\n", _buf);
#endif
  }

  return p;
}

EXPORT_ELEMENT(IPPrint)
