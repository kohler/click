/*
 * getip6address.{cc,hh} -- element sets IP6 destination annotation from
 * packet header
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
#include "getip6address.hh"
#include "confparse.hh"
#include "error.hh"
#include "click_ip6.h"

GetIP6Address::GetIP6Address()
  : Element(1, 1)
{
}


int
GetIP6Address::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  click_chatter("\n  ############# GetIP6Address start conf !\n");
   return cp_va_parse(conf, this, errh,
      cpUnsigned, "byte offset of IP6 address", &_offset,
       0);
}

Packet *
GetIP6Address::simple_action(Packet *p)
{
  //click_chatter("calling getip6address-smaction \n");
  click_chatter("IN getip6address: the starting of the des address: \n");
  IP6Address dst=IP6Address((unsigned char *)(p->data()+ _offset));
  click_chatter(dst.s());
  p->set_dst_ip6_anno(dst);
  click_chatter("after set-dst_ip6_anno in getip6address::smaction: \n");
  click_chatter(p->dst_ip6_anno().s());
  click_chatter("\n");
  return p;
}

//  void
//  GetIP6Address::push(int, Packet *p)
//  { 
//    smaction(p);
//    output(0).push(p);
//  }

//  Packet *
//  GetIP6Address::pull(int)
//  {
//    Packet *p = input(0).pull();
//    if(p)
//      smaction(p);
//    return(p);
//  }
 
EXPORT_ELEMENT(GetIP6Address)
