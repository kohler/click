/*
 * lookupiproute.{cc,hh} -- element looks up next-hop address in static
 * routing table
 * Robert Morris
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
#include "lookupiproute.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

LookupIPRoute::LookupIPRoute()
{
  add_input();
}

LookupIPRoute::~LookupIPRoute()
{
}

LookupIPRoute *
LookupIPRoute::clone() const
{
  return new LookupIPRoute;
}

int
LookupIPRoute::configure(const String &conf, ErrorHandler *errh)
{
  int maxout = 0, i;
  Vector<String> args;
  cp_argvec(conf, args);

  _t.clear();
  for (int i = 0; i < args.size(); i++) {
    String arg = args[i];
    unsigned int dst, mask, gw;
    int index;
    if (cp_ip_address(arg, (unsigned char *)&dst, &arg) &&
	cp_eat_space(arg) &&
        cp_ip_address(arg, (unsigned char *)&mask, &arg) &&
	cp_eat_space(arg) &&
        cp_ip_address(arg, (unsigned char *)&gw, &arg) &&
        cp_eat_space(arg) &&
        cp_integer(arg, index, &arg)){
      _t.add(dst, mask, gw, index);
      if(index > maxout)
        maxout = index;
    } else {
      errh->error("expects DST MASK GW INDEX");
      return -1;
    }
  }

  for(i = 0; i <= maxout; i++)
    add_output();
  
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
  unsigned gw = 0;
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
  
  if (_t.lookup(a.saddr(), gw, ifi) == true) {
#ifdef IP_RT_CACHE2
    _last_addr2 = _last_addr;
    _last_gw2 = _last_gw;
    _last_output2 = _last_output;
#endif
    _last_addr = a;
    _last_gw = gw;
    _last_output = ifi;
    if (gw != 0)
      p->set_dst_ip_anno(IPAddress(gw));
    output(ifi).push(p);
  } else {
    click_chatter("LookupIPRoute: no gw for %x", a.saddr());
    p->kill();
  }
}

EXPORT_ELEMENT(LookupIPRoute)
