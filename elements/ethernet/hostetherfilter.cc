/*
 * hostetherfilter.{cc,hh} -- Discard packets not for this host.
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
#include "hostetherfilter.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <click/click_ether.h>

HostEtherFilter::HostEtherFilter() : _drop_own(false)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

HostEtherFilter::~HostEtherFilter()
{
  MOD_DEC_USE_COUNT;
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
