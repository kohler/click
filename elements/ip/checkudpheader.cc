/*
 * checkudpheader.{cc,hh} -- element checks UDP header for correctness
 * (checksums, lengths)
 * Eddie Kohler
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
#include "checkudpheader.hh"
#include "click_ip.h"
#include "click_udp.h"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "bitvector.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

CheckUDPHeader::CheckUDPHeader()
  : _drops(0)
{
  add_input();
  add_output();
}

CheckUDPHeader *
CheckUDPHeader::clone() const
{
  return new CheckUDPHeader();
}

void
CheckUDPHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

void
CheckUDPHeader::processing_vector(Vector<int> &in_v, int in_offset,
				   Vector<int> &out_v, int out_offset) const
{
  in_v[in_offset+0] = out_v[out_offset+0] = AGNOSTIC;
  if (noutputs() == 2)
    out_v[out_offset+1] = PUSH;
}

Packet *
CheckUDPHeader::simple_action(Packet *p)
{
  click_ip *iph = p->ip_header();
  unsigned len, iph_len;
  click_udp *udph;
  
  if (!iph || iph->ip_p != IP_PROTO_UDP)
    goto bad;

  iph_len = iph->ip_hl << 2;
  udph = (click_udp *)((unsigned char *)iph + iph_len);
  len = ntohs(udph->uh_ulen);
  if (len < sizeof(click_udp)
      || p->length() < len + iph_len + ((unsigned char *)iph - p->data()))
    goto bad;

  if (udph->uh_sum != 0) {
    unsigned csum = ~in_cksum((unsigned char *)udph, len) & 0xFFFF;
#ifdef __KERNEL__
    if (csum_tcpudp_magic(iph->ip_src.s_addr, iph->ip_dst.s_addr,
			  len, IP_PROTO_UDP, csum) != 0)
      goto bad;
#else
    unsigned short *words = (unsigned short *)&iph->ip_src;
    csum += words[0];
    csum += words[1];
    csum += words[2];
    csum += words[3];
    csum += htons(IP_PROTO_UDP);
    csum += htons(len);
    while (csum >> 16)
      csum = (csum & 0xFFFF) + (csum >> 16);
    if (csum != 0xFFFF)
      goto bad;
#endif
  }

  return p;
  
 bad:
  if (_drops == 0)
    click_chatter("UDP checksum failed");
  _drops++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
  
  return 0;
}

static String
CheckUDPHeader_read_drops(Element *thunk, void *)
{
  CheckUDPHeader *e = (CheckUDPHeader *)thunk;
  return String(e->drops()) + "\n";
}

void
CheckUDPHeader::add_handlers()
{
  add_read_handler("drops", CheckUDPHeader_read_drops, 0);
}

EXPORT_ELEMENT(CheckUDPHeader)
