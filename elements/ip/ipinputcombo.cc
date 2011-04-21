/*
 * ipinputcombo.{cc,hh} -- IP router input combination element
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
#include "ipinputcombo.hh"
#include <clicknet/ip.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/standard/alignmentinfo.hh>
#include "elements/ip/checkipheader.hh"
CLICK_DECLS

IPInputCombo::IPInputCombo()
{
  _drops = 0;
}

IPInputCombo::~IPInputCombo()
{
}

int
IPInputCombo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	.read_mp("COLOR", _color)
	.read_p("BADSRC*", CheckIPHeader::OldBadSrcArg(), _bad_src)
	.read("INTERFACES", CheckIPHeader::InterfacesArg(), _bad_src, _good_dst)
	.read("BADSRC", _bad_src)
	.read("GOODDST", _good_dst)
	.complete() < 0)
	return -1;

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  // check alignment
  {
    int ans, c, o;
    ans = AlignmentInfo::query(this, 0, c, o);
    _aligned = (ans && c == 4 && o == 2);
    if (!_aligned)
      errh->warning("IP header unaligned, cannot use fast IP checksum");
    if (!ans)
      errh->message("(Try passing the configuration through `click-align'.)");
  }
#endif

  return 0;
}

inline Packet *
IPInputCombo::smaction(Packet *p)
{

  /* Paint */
  SET_PAINT_ANNO(p, _color);

  /* Strip(14) */
  p->pull(14);

  /* CheckIPHeader */
  const click_ip *ip = reinterpret_cast<const click_ip *>(p->data());
  unsigned hlen, len;

  if(p->length() < sizeof(click_ip))
    goto bad;

  if(ip->ip_v != 4)
    goto bad;

  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip))
    goto bad;

  len = ntohs(ip->ip_len);
  if (len > p->length() || len < hlen)
    goto bad;

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  if (_aligned) {
    if (ip_fast_csum((unsigned char *)ip, ip->ip_hl) != 0)
      goto bad;
  } else {
    if (click_in_cksum((unsigned char *)ip, hlen) != 0)
      goto bad;
  }
#elif HAVE_FAST_CHECKSUM
  if (ip_fast_csum((unsigned char *)ip, ip->ip_hl) != 0)
    goto bad;
#else
  if (click_in_cksum((unsigned char *)ip, hlen) != 0)
    goto bad;
#endif

  /*
   * RFC1812 5.3.7 and 4.2.2.11: discard illegal source addresses.
   * Configuration string should have listed all subnet
   * broadcast addresses known to this router.
   */
  if (find(_bad_src.begin(), _bad_src.end(), IPAddress(ip->ip_src)) < _bad_src.end()
      && find(_good_dst.begin(), _good_dst.end(), IPAddress(ip->ip_dst)) == _good_dst.end())
    goto bad;

  /*
   * RFC1812 4.2.3.1: discard illegal destinations.
   * We now do this in the IP routing table.
   */

  p->set_ip_header(ip, hlen);

  // shorten packet according to IP length field -- 7/28/2000
  if (p->length() > len)
    p->take(p->length() - len);

  // set destination IP address annotation
  p->set_dst_ip_anno(ip->ip_dst);

  return(p);

 bad:
  if(_drops == 0)
    click_chatter("IP checksum failed");
  p->kill();
  _drops++;
  return(0);
}

void
IPInputCombo::push(int, Packet *p)
{
  if((p = smaction(p)) != 0)
    output(0).push(p);
}

Packet *
IPInputCombo::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    p = smaction(p);
  return(p);
}

void
IPInputCombo::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(CheckIPHeader)
EXPORT_ELEMENT(IPInputCombo)
ELEMENT_MT_SAFE(IPInputCombo)
