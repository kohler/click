/*
 * hostetherfilter.{cc,hh} -- Discard packets not for this host.
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
#include "hostetherfilter.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"
#include "etheraddress.hh"
#include "click_ether.h"

HostEtherFilter::HostEtherFilter()
{
  add_input();
  add_output();
}

HostEtherFilter::~HostEtherFilter()
{
}

int
HostEtherFilter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return(cp_va_parse(conf, this, errh,
                     cpEthernetAddress, "Ethernet address", &_addr,
                     0));
}

HostEtherFilter *
HostEtherFilter::clone() const
{
  return new HostEtherFilter;
}

Packet *
HostEtherFilter::simple_action(Packet *p)
{
  click_ether *e = (click_ether *) p->data();

  if((e->ether_dhost[0] & 0x80) || memcmp(e->ether_dhost, _addr, 6) == 0){
    return(p);
  } else {
    p->kill();
    return(0);
  }
}

EXPORT_ELEMENT(HostEtherFilter)
