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

#include <click/config.h>
#include "checkipheader.hh"
#include <click/click_ip.h>
#include <click/ipaddressset.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include "elements/standard/alignmentinfo.hh"
#ifdef __KERNEL__
# include <net/checksum.h>
#endif

const char *CheckIPHeader::reason_texts[NREASONS] = {
  "tiny packet", "bad IP version", "bad IP header length",
  "bad IP length", "bad IP checksum", "bad source address"
};

CheckIPHeader::CheckIPHeader()
  : _bad_src(0), _reason_drops(0)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
  _drops = 0;
}

CheckIPHeader::~CheckIPHeader()
{
  MOD_DEC_USE_COUNT;
  delete[] _bad_src;
  delete[] _reason_drops;
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
  ips.insert(IPAddress(0));
  ips.insert(IPAddress(0xFFFFFFFFU));
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
    _reason_drops = new u_atomic32_t[NREASONS];

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
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

Packet *
CheckIPHeader::drop(Reason reason, Packet *p)
{
  if (_drops == 0 || _verbose)
    click_chatter("IP header check failed: %s", reason_texts[reason]);
  _drops++;

  if (_reason_drops)
    _reason_drops[reason]++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();

  return 0;
}

Packet *
CheckIPHeader::simple_action(Packet *p)
{
  const click_ip *ip = reinterpret_cast<const click_ip *>(p->data() + _offset);
  unsigned plen = p->length() - _offset;
  unsigned int src;
  unsigned hlen, len;

  // cast to int so very large plen is interpreted as negative 
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

  int val;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  if (_aligned)
    val = ip_fast_csum((unsigned char *)ip, ip->ip_hl);
  else
    val = in_cksum((unsigned char *)ip, hlen);
#elif HAVE_FAST_CHECKSUM
  val = ip_fast_csum((unsigned char *)ip, ip->ip_hl);
#else
  val = in_cksum((unsigned char *)ip, hlen);
#endif
  if (val != 0)
    return drop(BAD_CHECKSUM, p);

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

String
CheckIPHeader::read_handler(Element *e, void *thunk)
{
  CheckIPHeader *c = reinterpret_cast<CheckIPHeader *>(e);
  switch ((int)thunk) {

   case 0:			// drops
    return String(c->_drops) + "\n";

   case 1: {			// drop_details
     StringAccum sa;
     for (int i = 0; i < NREASONS; i++)
       sa << c->_reason_drops[i] << '\t' << reason_texts[i] << '\n';
     return sa.take_string();
   }

   default:
    return String("<error>\n");

  }
}

void
CheckIPHeader::add_handlers()
{
  add_read_handler("drops", read_handler, (void *)0);
  if (_reason_drops)
    add_read_handler("drop_details", read_handler, (void *)1);
}

EXPORT_ELEMENT(CheckIPHeader)
