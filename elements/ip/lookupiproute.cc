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
  int maxout = -1;
  _t.clear();
  
  Vector<String> args;
  cp_argvec(conf, args);

  int before = errh->nerrors();
  for (int i = 0; i < args.size(); i++) {
    String arg = args[i];
    unsigned int dst, mask, gw;
    int output_num;
    bool ok = false;
    if (cp_ip_address_mask(arg, (unsigned char *)&dst, (unsigned char *)&mask, &arg)) {
      cp_eat_space(arg);
      if (cp_ip_address(arg, (unsigned char *)&gw, &arg)) {
	ok = cp_eat_space(arg) && cp_integer(arg, output_num);
      } else {
	gw = 0;
	ok = cp_integer(arg, output_num);
      }
    }

    if (ok && output_num >= 0) {
      _t.add(dst, mask, gw, output_num);
      if (output_num > maxout)
        maxout = output_num;
    } else
      errh->error("argument %d should be `DADDR MASK [GATEWAY] OUTPUT'", i+1);
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
  
  if (_t.lookup(a.addr(), gw, ifi) == true) {
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
    click_chatter("LookupIPRoute: no gw for %x", a.addr());
    p->kill();
  }
}

EXPORT_ELEMENT(LookupIPRoute)
