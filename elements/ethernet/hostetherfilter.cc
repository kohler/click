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

HostEtherFilter::HostEtherFilter() : _drop_own(false)
{
  add_input();
  add_output();
}

HostEtherFilter::~HostEtherFilter()
{
}

void
HostEtherFilter::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
HostEtherFilter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return(cp_va_parse(conf, this, errh,
                     cpEthernetAddress, "Ethernet address", &_addr,
		     cpOptional,
		     cpBool, "Drop own packets?", &_drop_own,
                     0));
}

HostEtherFilter *
HostEtherFilter::clone() const
{
  return new HostEtherFilter;
}

Packet *
HostEtherFilter::drop(Packet *p)
{
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
  return 0;
}

Packet *
HostEtherFilter::simple_action(Packet *p)
{
  click_ether *e = (click_ether *) p->data();

  if (_drop_own && memcmp(e->ether_shost, _addr, 6) == 0)
    return drop(p);

  if ((e->ether_dhost[0] & 0x80) || memcmp(e->ether_dhost, _addr, 6) == 0)
    return p;
  else
    return drop(p);
}

EXPORT_ELEMENT(HostEtherFilter)
