/*
 * udpipencap.{cc,hh} -- element encapsulates packet in UDP/IP header
 * Benjie Chen, Eddie Kohler
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
#include <click/click_ip.h>
#include "udpipencap.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/standard/alignmentinfo.hh>
#ifdef CLICK_LINUXMODULE
# include <net/checksum.h>
#endif

UDPIPEncap::UDPIPEncap()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

UDPIPEncap::~UDPIPEncap()
{
  MOD_DEC_USE_COUNT;
}

UDPIPEncap *
UDPIPEncap::clone() const
{
  return new UDPIPEncap;
}

int
UDPIPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  bool do_cksum = true;
  unsigned sp, dp;
  if (cp_va_parse(conf, this, errh,
		  cpIPAddress, "source address", &_saddr,
		  cpUnsigned, "source port", &sp,
		  cpIPAddress, "destination address", &_daddr,
		  cpUnsigned, "destination port", &dp,
		  cpOptional,
		  cpBool, "do UDP checksum?", &do_cksum,
		  0) < 0)
    return -1;
  if (sp >= 0x10000 || dp >= 0x10000)
    return errh->error("source or destination port too large");
  
  _sport = sp;
  _dport = dp;
  _id = 0;
  _cksum = do_cksum;

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  // check alignment
  {
    int ans, c, o;
    ans = AlignmentInfo::query(this, 0, c, o);
    _aligned = (ans && c == 4 && o == 0);
    if (!_aligned)
      errh->warning("IP header unaligned, cannot use fast IP checksum");
    if (!ans)
      errh->message("(Try passing the configuration through `click-align'.)");
  }
#endif
  
  return 0;
}

Packet *
UDPIPEncap::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->push(sizeof(click_udp) + sizeof(click_ip));
  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

  // set up IP header
  ip->ip_v = 4;
  ip->ip_hl = sizeof(click_ip) >> 2;
  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id.read_and_add(1));
  ip->ip_p = IP_PROTO_UDP;
  ip->ip_src = _saddr;
  ip->ip_dst = _daddr;
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 250;

  ip->ip_sum = 0;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  if (_aligned)
    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
  else
    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#elif HAVE_FAST_CHECKSUM
  ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#else
  ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#endif
  
  p->set_dst_ip_anno(IPAddress(_daddr));
  p->set_ip_header(ip, sizeof(click_ip));

  // set up UDP header
  udp->uh_sport = htons(_sport);
  udp->uh_dport = htons(_dport);
  uint16_t len = p->length() - sizeof(click_ip);
  udp->uh_ulen = htons(len);
  udp->uh_sum = 0;
  if (_cksum) {
    unsigned csum = ~click_in_cksum((unsigned char *)udp, len) & 0xFFFF;
#ifdef CLICK_LINUXMODULE
    udp->uh_sum = csum_tcpudp_magic(_saddr.s_addr, _daddr.s_addr,
				    len, IP_PROTO_UDP, csum);
#else
    const uint16_t *words = (const uint16_t *)&ip->ip_src;
    csum += words[0];
    csum += words[1];
    csum += words[2];
    csum += words[3];
    csum += htons(IP_PROTO_UDP);
    csum += htons(len);
    while (csum >> 16)
      csum = (csum & 0xFFFF) + (csum >> 16);
    udp->uh_sum = ~csum & 0xFFFF;
#endif
  }
  
  return p;
}

EXPORT_ELEMENT(UDPIPEncap)
ELEMENT_MT_SAFE(UDPIPEncap)
