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
  return 0;
}

void
LookupIPRoute::push(int, Packet *p)
{
  IPAddress a = p->dst_ip_anno();
  unsigned gw = 0;
  int ifi = -1;

  if (a == _last_addr && a) {
    if (_last_gw)
      p->set_dst_ip_anno(_last_gw);
    output(_last_output).push(p);
    return;
  }
  
  if (_t.lookup(a.s_addr(), gw, ifi) == true) {
    _last_addr = a;
    _last_gw = gw;
    _last_output = ifi;
    if (gw != 0)
      p->set_dst_ip_anno(IPAddress(gw));
    output(ifi).push(p);
  } else {
    click_chatter("LookupIPRoute: no gw for %x", a.s_addr());
    p->kill();
  }
}

EXPORT_ELEMENT(LookupIPRoute)
