/*
 * ipfragmenter.{cc,hh} -- element fragments IP packets
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "ipfragmenter.hh"
#include <click/click_ip.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

IPFragmenter::IPFragmenter()
{
  MOD_INC_USE_COUNT;
  _fragments = 0;
  _mtu = 0;
  _drops = 0;
  add_input();
  add_output();
}

IPFragmenter::~IPFragmenter()
{
  MOD_DEC_USE_COUNT;
}

void
IPFragmenter::notify_noutputs(int n)
{
  // allow 2 outputs -- then packet is pushed onto 2d output instead of
  // dropped
  set_noutputs(n < 2 ? 1 : 2);
}

IPFragmenter *
IPFragmenter::clone() const
{
  return new IPFragmenter;
}


int
IPFragmenter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpUnsigned, "MTU", &_mtu,
		  0) < 0)
    return -1;
  return 0;
}

int
IPFragmenter::optcopy(const click_ip *ip1, click_ip *ip2)
{
  int opts = (ip1->ip_hl << 2) - sizeof(click_ip);
  u_char *base1 = (u_char *) (ip1 + 1);
  int optlen;
  int i2 = 0;
  u_char *base2 = (u_char *) (ip2 + 1);

  for (int i1 = 0; i1 < opts; i1 += optlen) {
    int opt = base1[i1];
    if(opt == IPOPT_EOL)
      break;
    if(opt == IPOPT_NOP){
      optlen = 1;
    } else {
      optlen = base1[i1+1];
    }

    if(opt & 0x80){
      /* copy it */
      if(ip2){
        memcpy(base2 + i2, base1 + i1, optlen);
      }
      i2 += optlen;
    }
  }

  for (; i2 & 3; i2++)
    if (ip2)
      base2[i2] = IPOPT_EOL;

  return i2;
}

void
IPFragmenter::fragment(Packet *p)
{
  const click_ip *ip = p->ip_header();
  assert(ip);
  
  int hlen = ip->ip_hl << 2;
  int len = (_mtu - hlen) & ~7;
  int ipoff = ntohs(ip->ip_off);
  if ((ipoff & IP_DF) || len < 8) {
    click_chatter("IPFragmenter(%d) DF %s %s len=%d",
                  _mtu,
                  IPAddress(ip->ip_src).s().cc(),
                  IPAddress(ip->ip_dst).s().cc(),
                  p->length());
    _drops++;
    if (noutputs() == 2)
      output(1).push(p);
    else
      p->kill();
    return;
  }

  int olen = optcopy(ip, 0);
  int h1len = sizeof(*ip) + olen;
  int plen = ntohs(ip->ip_len);
  int off;
  for(off = hlen + len; off < plen; off += len){
    int p1datalen = plen - off;
    if (p1datalen > len)
      p1datalen = len;
    int p1len = p1datalen + h1len;
    WritablePacket *p1 = Packet::make(p1len);
    click_ip *ip1 = reinterpret_cast<click_ip *>(p1->data());

    *ip1 = *ip;
    optcopy(ip, ip1);

    assert(off + p1datalen <= (int)p->length());
    memcpy(p1->data() + h1len, p->data() + off, p1datalen);
    
    ip1->ip_hl = h1len >> 2;
    ip1->ip_off = ((off - hlen) >> 3) + (ipoff & IP_OFFMASK);
    if(ipoff & IP_MF)
      ip1->ip_off |= IP_MF;
    if(off + p1datalen < plen)
      ip1->ip_off |= IP_MF;
    ip1->ip_off = htons(ip1->ip_off);
    ip1->ip_len = htons(p1len);
    ip1->ip_sum = 0;
    ip1->ip_sum = click_in_cksum(p1->data(), h1len);

    p1->copy_annotations(p);
    p1->set_ip_header(ip1, h1len);

    output(0).push(p1);
    _fragments++;
  }

  // XXX alignment???
  WritablePacket *q = p->uniqueify();
  q->take(q->length() - (hlen + len));
  click_ip *qip = reinterpret_cast<click_ip *>(q->data());
  qip->ip_len = htons(hlen + len);
  qip->ip_off = htons(ipoff | IP_MF);
  qip->ip_sum = 0;
  qip->ip_sum = click_in_cksum(reinterpret_cast<unsigned char *>(qip), hlen);
  q->set_ip_header(qip, sizeof(click_ip)); // XXX correct headerlength?
  output(0).push(q);
}

void
IPFragmenter::push(int, Packet *p)
{
  if (p->length() <= _mtu)
    output(0).push(p);
  else
    fragment(p);
}

static String
IPFragmenter_read_drops(Element *xf, void *)
{
  IPFragmenter *f = (IPFragmenter *)xf;
  return String(f->drops()) + "\n";
}

static String
IPFragmenter_read_fragments(Element *xf, void *)
{
  IPFragmenter *f = (IPFragmenter *)xf;
  return String(f->fragments()) + "\n";
}

void
IPFragmenter::add_handlers()
{
  add_read_handler("drops", IPFragmenter_read_drops, 0);
  add_read_handler("fragments", IPFragmenter_read_fragments, 0);
}


EXPORT_ELEMENT(IPFragmenter)
ELEMENT_MT_SAFE(IPFragmenter)
