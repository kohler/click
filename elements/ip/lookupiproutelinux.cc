/*
 * lookupiproutelinux.{cc,hh} -- element looks up next-hop address in
 * Linux's routing table
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
#include <click/package.hh>
#include "lookupiproutelinux.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

#ifdef CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/netdevice.h>
# include <linux/ip.h>
# include <net/route.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif

LookupIPRouteLinux::LookupIPRouteLinux()
{
  MOD_INC_USE_COUNT;
  add_input();

  _nout = 0;
#ifdef __KERNEL__
  _out2dev = 0;
#endif
}

LookupIPRouteLinux::~LookupIPRouteLinux()
{
  MOD_DEC_USE_COUNT;
#ifdef __KERNEL__
  if(_out2dev)
    delete[] _out2dev;
#endif
}

int
LookupIPRouteLinux::configure(const Vector<String> &conf, ErrorHandler *)
{
  _out2devname = conf;
  _nout = _out2devname.size();
  set_noutputs(_nout + 1);
  return 0;
}

int
LookupIPRouteLinux::initialize(ErrorHandler *errh)
{
  if(init_routes(errh) < 0)
    return(-1);

  return 0;
}

LookupIPRouteLinux *
LookupIPRouteLinux::clone() const
{
  return new LookupIPRouteLinux;
}

#ifdef __KERNEL__

int
LookupIPRouteLinux::init_routes(ErrorHandler *errh)
{
  if(_out2dev)
    delete[] _out2dev;
  _out2dev = new net_device * [_nout];
  _out2dev[0] = 0;
  int i;
  for(i = 0; i < _nout; i++){
    net_device *dev = dev_get_by_name(_out2devname[i].cc());
    if(dev == 0)
      return errh->error("Cannot find device %s", _out2devname[i].cc());
  }
  return(0);
}

bool
LookupIPRouteLinux::lookup(IPAddress a, IPAddress &gw, int &ifi)
{
  struct rtable *rt = 0;

  if (ip_route_output(&rt,
                      a.addr(), /* dst */
                      0,          /* src */
                      0,          /* tos */
                      0) == 0){
    gw = IPAddress(rt->rt_gateway);
    ifi = -1;
    net_device *dev = rt->u.dst.dev;
    int i;
    for(i = 0; i < _nout; i++){
      if(_out2dev[i] == dev)
        ifi = i;
    }
    if(ifi == -1)
      click_chatter("LookupIPRouteLinux: unknown dev %x", dev);
    ip_rt_put(rt);
    return(true);
  }
  return(false);
}

#else /* !__KERNEL__ */

int
LookupIPRouteLinux::init_routes(ErrorHandler *errh)
{
  _t.clear();

  FILE *fp;
  char buf[200];
  fp = fopen("/proc/net/route", "r");
  if(fp == 0)
    return errh->error("cannot open /proc/net/route");
  while(fgets(buf, sizeof(buf), fp)){
    char devname[200];
    unsigned int dst, gw, mask;
    // Iface Destination Gateway Flags RefCnt Use Metric Mask MTU Window IRTT
    if(sscanf(buf, "%s%x%x%*s%*s%*s%*s%x",
              devname,
              &dst,
              &gw,
              &mask) == 4){
      int i;
      for(i = 0; i < _nout; i++)
        if(_out2devname[i] == devname)
          break;
      if(i < _nout){
        _t.add(dst, mask, gw, i);
      } else {
        click_chatter("LookupIPRouteLinux: no output for dev %s", devname);
      }
    }
  }
  
  fclose(fp);
  return(0);
}

bool
LookupIPRouteLinux::lookup(IPAddress a, IPAddress &gw, int &ifi)
{
  return _t.lookup(a, gw, ifi);
}

#endif /* __KERNEL__ */

void
LookupIPRouteLinux::push(int, Packet *p)
{
  IPAddress a = p->dst_ip_anno();
  IPAddress gw;
  int ifi = -1;

  if (lookup(a, gw, ifi) == true){
    click_chatter("routed %x to %x %d",
                a.addr(),
                gw.addr(),
                ifi);
    if(gw.addr() != 0)
      p->set_dst_ip_anno(gw);
    output(ifi).push(p);
  } else {
    click_chatter("LookupIPRouteLinux: no gw for %x", a.addr());
    output(_nout).push(p);
  }
}

EXPORT_ELEMENT(LookupIPRouteLinux)
