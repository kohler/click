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
#include "lookupip6route.hh"
#include "ip6address.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
 
LookupIP6Route::LookupIP6Route()
{
  add_input();
}

LookupIP6Route::~LookupIP6Route()
{
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
  click_chatter(" \n lookupip6route::configure, arg.size is : %d -b ", conf.size());
  for (int i = 0; i < conf.size(); i++) {
    String arg = conf[i];
    //click_chatter("arg %d is  %s \n", i, arg.data());
    //struct in6my_addr dst, mask, gw;
    IP6Address dst, mask, gw;
    int index;
    //click_chatter("", arg);
    if (cp_ip6_address(arg, (unsigned char *)&dst, &arg) &&
	cp_eat_space(arg) &&
        cp_ip6_address(arg, (unsigned char *)&mask, &arg) &&
	cp_eat_space(arg) &&
        cp_ip6_address(arg, (unsigned char *)&gw, &arg) &&
        cp_eat_space(arg) &&
	cp_integer(arg, &index)) {
        //cp_integer(arg, index, &arg)){
      _t.add(dst, mask, gw, index);
      click_chatter(" \n lookupip6route::configure -dst= ");
      ((IP6Address)dst).print();
   
      if(index > maxout)
        maxout = index;
    } else {
      click_chatter(" \n index is %d, maxout is %d", index, maxout);
      errh->error("\n expects DST MASK GW INDEX");
    }
  }

  //  for(i = 0; i <= maxout; i++)
//      add_output(); 

  if (errh->nerrors()!=before) 
    return -1;
  if (maxout <0)
    errh->warning("no routes");
  
  set_noutputs(maxout +1);
  
  click_chatter("\n ############ LookupIP6Route conf successful ! \n");
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
  // click_chatter("Lookupip6route::push starts");
  IP6Address a = p->dst_ip6_anno();
  //click_chatter("\n packet's dst = ");  
  //a.print();
  //click_chatter("\n");
  
  IP6Address gw = IP6Address(new ip6_addr());
  int ifi = -1;

  if (a) {
    if (a == _last_addr     ) {
      if (_last_gw)
	p->set_dst_ip6_anno(_last_gw);
      output(_last_output).push(p);
    //click_chatter("push in LookupIP6Route successful");
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
      if (_last_gw2)
	p->set_dst_ip6_anno(_last_gw2);
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
    //click_chatter("another direction in push of lookup ip6Route \n");
    _last_addr = a;
    _last_gw = gw;
    //click_chatter("gateway assigned is \n");
    //IP6Address(gw).print();
    _last_output = ifi;
    if (gw != 0)
      p->set_dst_ip6_anno(IP6Address(gw));
    output(ifi).push(p);
    
  } else {
    click_chatter("LookupIP6Route: no gw for %x", a.addr());
    p->kill();
  }
}

EXPORT_ELEMENT(LookupIP6Route)











