#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "lookupiproutelinux.hh"
#include "ipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

LookupIPRouteLinux::LookupIPRouteLinux()
{
  add_input();

  _nout = 0;
#ifdef __KERNEL__
  _out2dev = 0;
#endif
}

LookupIPRouteLinux::~LookupIPRouteLinux()
{
#ifdef __KERNEL__
  if(_out2dev)
    delete[] _out2dev;
#endif
}

int
LookupIPRouteLinux::configure(const String &conf, Router *, ErrorHandler *)
{
  cp_argvec(conf, _out2devname);
  _nout = _out2devname.size();
  add_outputs(_nout + 1);
  return 0;
}

int
LookupIPRouteLinux::initialize(Router *, ErrorHandler *errh)
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
  _out2dev = new struct device * [_nout];
  _out2dev[0] = 0;
  int i;
  for(i = 0; i < _nout; i++){
    struct device *dev = dev_get(_out2devname[i].cc());
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
                      a.s_addr(), /* dst */
                      0,          /* src */
                      0,          /* tos */
                      0) == 0){
    gw = IPAddress(rt->rt_gateway);
    ifi = -1;
    struct device *dev = rt->u.dst.dev;
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
  unsigned xgw;

  if(_t.lookup(a.s_addr(), xgw, ifi)){
    gw = IPAddress(xgw);
    return(true);
  }
  return(false);
}

#endif /* __KERNEL__ */

void
LookupIPRouteLinux::push(int, Packet *p)
{
  IPAddress a = p->dst_ip_anno();
  IPAddress gw;
  int ifi = -1;

  if(lookup(a, gw, ifi) == true){
    click_chatter("routed %x to %x %d",
                a.s_addr(),
                gw.s_addr(),
                ifi);
    if(gw.s_addr() != 0)
      p->set_dst_ip_anno(gw);
    output(ifi).push(p);
  } else {
    click_chatter("LookupIPRouteLinux: no gw for %x", a.s_addr());
    output(_nout).push(p);
  }
}

EXPORT_ELEMENT(LookupIPRouteLinux)
