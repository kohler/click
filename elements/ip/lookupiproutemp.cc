/*
 * lookupiproutemp.{cc,hh} -- element looks up next-hop address in static
 * routing table, has processor local cache in SMP mode.
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
#include "lookupiproutemp.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

LookupIPRouteMP::LookupIPRouteMP()
{
  MOD_INC_USE_COUNT;
  add_input();
}

LookupIPRouteMP::~LookupIPRouteMP()
{
  MOD_DEC_USE_COUNT;
}

LookupIPRouteMP *
LookupIPRouteMP::clone() const
{
  return new LookupIPRouteMP;
}

int
LookupIPRouteMP::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int maxout = -1;
  _t.clear();
  
  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    unsigned int dst, mask, gw;
    int output_num;
    bool ok = false;

    Vector<String> words;
    cp_spacevec(conf[i], words);
    
    if ((words.size() == 2 || words.size() == 3)
	&& cp_ip_prefix(words[0], (unsigned char *)&dst, (unsigned char *)&mask, true, this) // allow base IP addresses
	&& cp_integer(words.back(), &output_num)) {
      if (words.size() == 3)
	ok = cp_ip_address(words[1], (unsigned char *)&gw, this);
      else
	ok = true;
    }

    if (ok && output_num >= 0) {
      _t.add(dst, mask, gw, output_num);
      if (output_num > maxout)
        maxout = output_num;
    } else
      errh->error("argument %d should be `DADDR/MASK [GATEWAY] OUTPUT'", i+1);
  }
  if (errh->nerrors() != before)
    return -1;
  if (maxout < 0)
    errh->warning("no routes");

  set_noutputs(maxout + 1);
  return 0;
}

int
LookupIPRouteMP::initialize(ErrorHandler *)
{
  click_chatter("LookupIPRouteMP alignment: %p, %p", 
                &(_cache[0]._last_addr_1), &(_cache[1]._last_addr_1));
  for (int i=0; i<_cache_buckets; i++) {
    _cache[i]._last_addr_1 = IPAddress();
    _cache[i]._last_addr_2 = IPAddress();
  }
  return 0;
}

#ifdef __KERNEL__
extern "C" {
#include <linux/sched.h>
}
#endif

void
LookupIPRouteMP::push(int, Packet *p)
{
  IPAddress a = p->dst_ip_anno();
  unsigned gw = 0;
  int ifi = -1;
#ifdef __KERNEL__
  int bucket = current->processor % _cache_buckets;
#else
  int bucket = 0;
#endif

  if (a) {
    if (a == _cache[bucket]._last_addr_1) {
      if (_cache[bucket]._last_gw_1)
	p->set_dst_ip_anno(_cache[bucket]._last_gw_1);
      output(_cache[bucket]._last_output_1).push(p);
      return;
    } 
    else if (a == _cache[bucket]._last_addr_2) {
      if (_cache[bucket]._last_gw_2)
	p->set_dst_ip_anno(_cache[bucket]._last_gw_2);
      output(_cache[bucket]._last_output_2).push(p);
      return;
    } 
  }
  
  if (_t.lookup(a.addr(), gw, ifi) == true) {
    _cache[bucket]._last_addr_2 = _cache[bucket]._last_addr_1;
    _cache[bucket]._last_gw_2 = _cache[bucket]._last_gw_1;
    _cache[bucket]._last_output_2 = _cache[bucket]._last_output_1;
    _cache[bucket]._last_addr_1 = a;
    _cache[bucket]._last_gw_1 = gw;
    _cache[bucket]._last_output_1 = ifi;
    if (gw != 0)
      p->set_dst_ip_anno(IPAddress(gw));
    output(ifi).push(p);
  } else {
    click_chatter("LookupIPRouteMP: no gw for %x", a.addr());
    p->kill();
  }
}

EXPORT_ELEMENT(LookupIPRouteMP)
