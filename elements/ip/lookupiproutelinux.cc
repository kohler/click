/*
 * lookupiproutelinux.{cc,hh} -- element looks up next-hop address in
 * Linux's routing table
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

// N.B. does not work in kernel because of odd handling of _out2dev; should
// pay attention to device up/down and do reference counting correctly

#include <click/config.h>
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

CLICK_DECLS

LinuxIPLookup::LinuxIPLookup()
{
  _nout = 0;
#ifdef CLICK_LINUXMODULE
  _out2dev = 0;
#endif
}

LinuxIPLookup::~LinuxIPLookup()
{
#ifdef CLICK_LINUXMODULE
  if(_out2dev)
    delete[] _out2dev;
#endif
}

int
LinuxIPLookup::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _out2devname = conf;
  _nout = _out2devname.size();
  if (noutputs() < _nout || noutputs() > _nout + 1)
      return errh->error("need %d or %d output ports", _nout, _nout + 1);
  return 0;
}

int
LinuxIPLookup::initialize(ErrorHandler *errh)
{
  if(init_routes(errh) < 0)
    return(-1);

  return 0;
}

#ifdef CLICK_LINUXMODULE

int
LinuxIPLookup::init_routes(ErrorHandler *errh)
{
  if (_out2dev)
    delete[] _out2dev;
  _out2dev = new net_device * [_nout];
  _out2dev[0] = 0;
  int i;
  for(i = 0; i < _nout; i++){
      net_device *dev = AnyDevice::get_by_name(_out2devname[i].c_str());
    if (dev == 0)
      return errh->error("Cannot find device %s", _out2devname[i].c_str());
    _out2dev[i] = dev;
  }
  return(0);
}

bool
LinuxIPLookup::lookup(IPAddress a, IPAddress &gw, int &ifi)
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
      click_chatter("LinuxIPLookup: unknown dev %x", dev);
    ip_rt_put(rt);
    return(true);
  }
  return(false);
}

#else /* !CLICK_LINUXMODULE */

int
LinuxIPLookup::init_routes(ErrorHandler *errh)
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
        click_chatter("LinuxIPLookup: no output for dev %s", devname);
      }
    }
  }

  fclose(fp);
  return(0);
}

bool
LinuxIPLookup::lookup(IPAddress a, IPAddress &gw, int &ifi)
{
  return _t.lookup(a, gw, ifi);
}

#endif /* CLICK_LINUXMODULE */

void
LinuxIPLookup::push(int, Packet *p)
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
    click_chatter("LinuxIPLookup: no gw for %x", a.addr());
    checked_output_push(_nout, p);
  }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LinuxIPLookup)
