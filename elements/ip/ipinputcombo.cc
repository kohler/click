/*
 * ipinputcombo.{cc,hh} -- IP router input combination element
 * Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "ipinputcombo.hh"
#include "click_ip.h"
#include "ipaddressset.hh"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "elements/standard/alignmentinfo.hh"

IPInputCombo::IPInputCombo()
  : _drops(0), _bad_src(0)
{
  add_input();
  add_output();
}

IPInputCombo::~IPInputCombo()
{
  delete[] _bad_src;
}

IPInputCombo *
IPInputCombo::clone() const
{
  return new IPInputCombo();
}

int
IPInputCombo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  IPAddressSet ips;
  ips.insert(0);
  ips.insert(0xFFFFFFFFU);
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "color", &_color,
		  cpOptional,
		  cpIPAddressSet, "bad source addresses", &ips,
		  0) < 0)
    return -1;

  delete[] _bad_src;
  _n_bad_src = ips.size();
  _bad_src = ips.list_copy();

#ifdef __KERNEL__
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
  unsigned int src;

  /* Paint */
  p->set_color_anno(_color);

  /* Strip(14) */
  p->pull(14);

  /* GetIPAddress(16) */
  p->set_dst_ip_anno(IPAddress(p->data() + 16));

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
  
#ifdef __KERNEL__
  if (_aligned) {
    if (ip_fast_csum((unsigned char *)ip, ip->ip_hl) != 0)
      goto bad;
  } else {
#endif
  if (in_cksum((unsigned char *)ip, hlen) != 0)
    goto bad;
#ifdef __KERNEL__
  }
#endif

  /*
   * RFC1812 5.3.7 and 4.2.2.11: discard illegal source addresses.
   * Configuration string should have listed all subnet
   * broadcast addresses known to this router.
   */
  src = ip->ip_src.s_addr;
  for(int i = 0; i < _n_bad_src; i++)
    if(src == _bad_src[i])
      goto bad;

  /*
   * RFC1812 4.2.3.1: discard illegal destinations.
   * We now do this in the IP routing table.
   */

  p->set_ip_header(ip, hlen);

  // shorten packet according to IP length field -- 7/28/2000
  if (p->length() > len)
    p->take(p->length() - len);
  
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

static String
IPInputCombo_read_drops(Element *xf, void *)
{
  IPInputCombo *f = (IPInputCombo *)xf;
  return String(f->drops()) + "\n";
}

void
IPInputCombo::add_handlers()
{
  add_read_handler("drops", IPInputCombo_read_drops, 0);
}

EXPORT_ELEMENT(IPInputCombo)
