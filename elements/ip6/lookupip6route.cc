/*
 * lookupip6route.{cc,hh} -- element looks up next-hop ip6 address in static
 * routing table
 * Peilei Fan, Robert Morris
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
#include <click/package.hh>
#include "lookupip6route.hh"
#include <click/ip6address.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
 
LookupIP6Route::LookupIP6Route()
{
  MOD_INC_USE_COUNT;
  add_input();
}

LookupIP6Route::~LookupIP6Route()
{
  MOD_DEC_USE_COUNT;
}

LookupIP6Route *
LookupIP6Route::clone() const
{
  return new LookupIP6Route;
}

int
LookupIP6Route::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int maxout = -1;
  _t.clear();

  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    IP6Address dst, mask, gw;
    int output_num;
    bool ok = false;

    Vector<String> words;
    cp_spacevec(conf[i], words);
   
    if ((words.size()==2 || words.size()==3 )
      && cp_ip6_prefix(words[0], (unsigned char *)&dst, (unsigned char *)&mask, true, this) 
	&& cp_integer(words.back(), &output_num))
    { 
      if (words.size()==3)
	ok = cp_ip6_address(words[1], (unsigned char *)&gw, this);
      else {
	gw = IP6Address("::0");
	ok = true;
      }
    }

  if (ok && output_num>=0) {
    _t.add(dst, mask, gw, output_num); 
    if( output_num > maxout) 
        maxout = output_num;
    } else {
      errh->error("argument %d should be DADDR/MASK [GW] OUTPUT", i+1);
    }
  }


  if (errh->nerrors()!=before) 
    return -1;
  if (maxout <0)
    errh->warning("no routes");
  
  set_noutputs(maxout +1);
  return 0;

}

int
LookupIP6Route::initialize(ErrorHandler *)
{
  _last_addr = IP6Address();
  #ifdef IP_RT_CACHE2
  _last_addr2 = _last_addr;
#endif
  return 0;
}

void
LookupIP6Route::push(int, Packet *p)
{
 
  IP6Address a = p->dst_ip6_anno();
  IP6Address gw;
  int ifi = -1;
  
  if (a) {
    if (a == _last_addr     ) {
      if (_last_gw)
	{
	  p->set_dst_ip6_anno(_last_gw);
	}
      output(_last_output).push(p);
      return;
    }
 #ifdef IP_RT_CACHE2
    else if (a == _last_addr2) {
#if 0
      IP6address tmpa; 
      int tmpi;
      EXCHANGE(_last_addr, _last_addr2, tmpa);
      EXCHANGE(_last_gw, _last_gw2, tmpa);
      EXCHANGE(_last_output, _last_output2, tmpi);
#endif
      if (_last_gw2) {
	p->set_dst_ip6_anno(_last_gw2); }
      output(_last_output2).push(p);
      return;
    }
#endif
  }
  
 
  if (_t.lookup(IP6Address(a.addr()), gw, ifi) == true) {
#ifdef IP_RT_CACHE2
    _last_addr2 = _last_addr;
    _last_gw2 = _last_gw;
    _last_output2 = _last_output;
#endif
   
    _last_addr = a;
    _last_gw = gw;
    _last_output = ifi;
    if (gw != IP6Address("::0"))
      {
	p->set_dst_ip6_anno(IP6Address(gw));
      }
    output(ifi).push(p);
    
  } else {
    p->kill();
  }
}

EXPORT_ELEMENT(LookupIP6Route)











