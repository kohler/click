/*
 * lookupiproute.{cc,hh} -- element looks up next-hop address in static
 * routing table
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
#include "lookupiproute.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

LookupIPRoute::LookupIPRoute()
{
  MOD_INC_USE_COUNT;
  add_input();
}

LookupIPRoute::~LookupIPRoute()
{
  MOD_DEC_USE_COUNT;
}

LookupIPRoute *
LookupIPRoute::clone() const
{
  return new LookupIPRoute;
}

int
LookupIPRoute::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int maxout = -1;
  _t.clear();
  
  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    uint32_t dst, mask, gw = 0;
    int32_t output_num;
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
LookupIPRoute::initialize(ErrorHandler *)
{
  _last_addr = IPAddress();
#ifdef IP_RT_CACHE2
  _last_addr2 = _last_addr;
#endif
  return 0;
}

void
LookupIPRoute::push(int, Packet *p)
{
#define EXCHANGE(a,b,t) { t = a; a = b; b = t; }
  IPAddress a = p->dst_ip_anno();
  IPAddress gw;
  int ifi = -1;

  if (a) {
    if (a == _last_addr) {
      if (_last_gw)
	p->set_dst_ip_anno(_last_gw);
      output(_last_output).push(p);
      return;
    } 
#ifdef IP_RT_CACHE2
    else if (a == _last_addr2) {
#if 0
      IPAddress tmpa; 
      int tmpi;
      EXCHANGE(_last_addr, _last_addr2, tmpa);
      EXCHANGE(_last_gw, _last_gw2, tmpa);
      EXCHANGE(_last_output, _last_output2, tmpi);
#endif
      if (_last_gw2)
	p->set_dst_ip_anno(_last_gw2);
      output(_last_output2).push(p);
      return;
    }
#endif
  }
  
  if (_t.lookup(a, gw, ifi) == true) {
#ifdef IP_RT_CACHE2
    _last_addr2 = _last_addr;
    _last_gw2 = _last_gw;
    _last_output2 = _last_output;
#endif
    _last_addr = a;
    _last_gw = gw;
    _last_output = ifi;
    if (gw)
      p->set_dst_ip_anno(IPAddress(gw));
    output(ifi).push(p);
  } else {
    click_chatter("LookupIPRoute: no gw for %x", a.addr());
    p->kill();
  }
}

EXPORT_ELEMENT(LookupIPRoute)
