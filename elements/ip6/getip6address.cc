/*
 * getip6address.{cc,hh} -- element sets IP6 destination annotation from
 * packet header
 * Peilei Fan 
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
   return cp_va_parse(conf, this, errh,
      cpUnsigned, "byte offset of IP6 address", &_offset,
       0);
}

Packet *
GetIP6Address::simple_action(Packet *p)
{
  
  IP6Address dst=IP6Address((unsigned char *)(p->data()+ _offset));
  p->set_dst_ip6_anno(dst); 
  return p;
}

 
EXPORT_ELEMENT(GetIP6Address)
