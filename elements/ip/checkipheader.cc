/*
 * checkipheader.{cc,hh} -- element checks IP header for correctness
 * (checksums, lengths, source addresses)
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
#include "checkipheader.hh"
#include "click_ip.h"
#include "ipaddressset.hh"
#include "glue.hh"
#include "confparse.hh"
#include "error.hh"
#include "elements/standard/alignmentinfo.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

CheckIPHeader::CheckIPHeader()
  : _bad_src(0), _drops(0)
{
  add_input();
  add_output();
}

CheckIPHeader::~CheckIPHeader()
{
  delete[] _bad_src;
}

CheckIPHeader *
CheckIPHeader::clone() const
{
  return new CheckIPHeader();
}

void
CheckIPHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
CheckIPHeader::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  IPAddressSet ips;
  ips.insert(0);
  ips.insert(0xFFFFFFFFU);
  _offset = 0;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpIPAddressSet, "bad source addresses", &ips,
		  cpUnsigned, "IP header offset", &_offset,
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
    o = (o + 4 - (_offset % 4)) % 4;
    _aligned = (ans && c == 4 && o == 0);
    if (!_aligned)
      errh->warning("IP header unaligned, cannot use fast IP checksum");
    if (!ans)
      errh->message("(Try passing the configuration through `click-align'.)");
  }
#endif
  
  return 0;
}

void
CheckIPHeader::drop_it(Packet *p)
{
  if (_drops == 0)
    click_chatter("IP header check failed");
  _drops++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
}

Packet *
CheckIPHeader::simple_action(Packet *p)
{
  const click_ip *ip = reinterpret_cast<const click_ip *>(p->data() + _offset);
  unsigned plen = p->length() - _offset;
  unsigned int src;
  unsigned hlen, len;
  
  if ((int)plen < (int)sizeof(click_ip))
    goto bad;

  if (ip->ip_v != 4)
    goto bad;
  
  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip))
    goto bad;
  
  len = ntohs(ip->ip_len);
  if (len > plen || len < hlen)
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
  if (plen > len)
    p->take(plen - len);
  
  return(p);
  
 bad:
  drop_it(p);
  return 0;
}

static String
CheckIPHeader_read_drops(Element *xf, void *)
{
  CheckIPHeader *f = (CheckIPHeader *)xf;
  return String(f->drops()) + "\n";
}

void
CheckIPHeader::add_handlers()
{
  add_read_handler("drops", CheckIPHeader_read_drops, 0);
}

EXPORT_ELEMENT(CheckIPHeader)
