/*
 * checkip6header.{cc,hh} -- element checks IP6 header for correctness
 * (lengths, source addresses)
 * Robert Morris , Peilei Fan
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
#include "checkip6header.hh"
#include <clicknet/ip6.h>
#include <click/ip6address.hh>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

CheckIP6Header::CheckIP6Header()
  : _bad_src(0), _drops(0)
{
}

CheckIP6Header::~CheckIP6Header()
{
  delete[] _bad_src;
}

int
CheckIP6Header::configure(Vector<String> &conf, ErrorHandler *errh)
{
 String badaddrs = String::make_empty();
 _offset = 0;
 Vector<String> ips;
 // ips.push_back("0::0"); // this address is only bad if we are a router
 ips.push_back("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); // bad IP6 address

 if (Args(conf, this, errh)
     .read_p("BADADDRS", badaddrs)
     .read_p("OFFSET", _offset)
     .complete() < 0)
    return -1;

  if (badaddrs) {
    Vector<String> words;
    cp_spacevec(badaddrs, words);
    IP6Address a;
    for (int j = 0; j < words.size(); j++) {
      if (!cp_ip6_address(words[j], (unsigned char *)&a)) {
	return errh->error("expects IP6ADDRESS -a ");
      }
      for (int j = 0; j < ips.size(); j++) {
	IP6Address b = IP6Address(ips[j]);
	if (b == a)
	  goto repeat;
      }
      ips.push_back(a.s());
     repeat: ;
    }
  }

  _n_bad_src = ips.size();
  _bad_src = new IP6Address [_n_bad_src];

  for (int i = 0; i<_n_bad_src; i++) {
    _bad_src[i]= IP6Address(ips[i]);
  }

  return 0;
}

void
CheckIP6Header::drop_it(Packet *p)
{
  if (_drops == 0)
    click_chatter("IP6 header check failed");
  _drops++;

  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
}

Packet *
CheckIP6Header::simple_action(Packet *p)
{
  const click_ip6 *ip = reinterpret_cast <const click_ip6 *>( p->data() + _offset);
  unsigned plen = p->length() - _offset;
  class IP6Address src;

  // check if the packet is smaller than ip6 header
  // cast to int so very large plen is interpreted as negative
  if((int)plen < (int)sizeof(click_ip6))
    goto bad;

 // check version
  if(ip->ip6_v != 6)
    goto bad;

  // check if the PayloadLength field is valid
   if(ntohs(ip->ip6_plen) > (plen-40))
     goto bad;

  /*
   * discard illegal source addresses.
   * Configuration string should have listed all subnet
   * broadcast addresses known to this router.
   */
   src=ip->ip6_src;
   for(int i = 0; i < _n_bad_src; i++) {
     if(src == _bad_src[i])
       goto bad;
   }

  /*
   * discard illegal destinations.
   * We will do this in the IP6 routing table.
   *
   *
   */

  p->set_ip6_header(ip);

  // shorten packet according to IP6 payload length field
  if(ntohs(ip->ip6_plen) < (plen-40))
    p->take(plen - 40 - ntohs(ip->ip6_plen));
  return(p);

 bad:
  drop_it(p);
  return 0;
}


static String
CheckIP6Header_read_drops(Element *xf, void *)
{
  CheckIP6Header *f = (CheckIP6Header *)xf;
  return String(f->drops());
}

void
CheckIP6Header::add_handlers()
{
  add_read_handler("drops", CheckIP6Header_read_drops);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckIP6Header)
