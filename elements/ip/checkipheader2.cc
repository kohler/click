/*
 * checkipheader2.{cc,hh} -- element checks IP header for correctness
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
#include <click/config.h>
#include <click/package.hh>
#include "checkipheader2.hh"
#include <click/click_ip.h>
#include <click/ipaddressset.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>

CheckIPHeader2::CheckIPHeader2()
{
  MOD_INC_USE_COUNT;
  // other stuff belongs to CheckIPHeader
}

CheckIPHeader2::~CheckIPHeader2()
{
  MOD_DEC_USE_COUNT;
  // other stuff belongs to CheckIPHeader
}

CheckIPHeader2 *
CheckIPHeader2::clone() const
{
  return new CheckIPHeader2();
}

int
CheckIPHeader2::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  // same as CheckIPHeader's configure, only don't check alignment
  
  IPAddressSet ips;
  ips.insert(0);
  ips.insert(0xFFFFFFFFU);
  _offset = 0;
  bool verbose = false;
  bool details = false;
  
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpIPAddressSet, "bad source addresses", &ips,
		  cpUnsigned, "IP header offset", &_offset,
		  cpKeywords,
		  "VERBOSE", cpBool, "be verbose?", &verbose,
		  "DETAILS", cpBool, "keep detailed counts?", &details,
		  0) < 0)
    return -1;
  
  delete[] _bad_src;
  _n_bad_src = ips.size();
  _bad_src = ips.list_copy();
  
  _verbose = verbose;
  if (details)
    _reason_drops = new int[NREASONS];

  return 0;
}

inline Packet *
CheckIPHeader2::smaction(Packet *p)
{
  const click_ip *ip = reinterpret_cast<const click_ip *>(p->data() + _offset);
  unsigned plen = p->length() - _offset;
  unsigned int src;
  unsigned hlen, len;
  
  if ((int)plen < (int)sizeof(click_ip))
    return drop(MINISCULE_PACKET, p);
  
  if (ip->ip_v != 4)
    return drop(BAD_VERSION, p);
  
  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip))
    return drop(BAD_HLEN, p);
  
  len = ntohs(ip->ip_len);
  if (len > plen || len < hlen)
    return drop(BAD_IP_LEN, p);

  /*
   * RFC1812 5.3.7 and 4.2.2.11: discard illegal source addresses.
   * Configuration string should have listed all subnet
   * broadcast addresses known to this router.
   */
  src = ip->ip_src.s_addr;
  for(int i = 0; i < _n_bad_src; i++)
    if(src == _bad_src[i])
      return drop(BAD_SADDR, p);

  /*
   * RFC1812 4.2.3.1: discard illegal destinations.
   * We now do this in the IP routing table.
   */

  p->set_ip_header(ip, hlen);

  // shorten packet according to IP length field -- 7/28/2000
  if (plen > len)
    p->take(plen - len);
  
  return(p);
}

void
CheckIPHeader2::push(int, Packet *p)
{
  if ((p = smaction(p)))
    output(0).push(p);
}

Packet *
CheckIPHeader2::pull(int)
{
  Packet *p = input(0).pull();
  if (p)
    p = smaction(p);
  return p;
}

EXPORT_ELEMENT(CheckIPHeader2)
