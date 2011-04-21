/*
 * arpfaker.{cc,hh} -- ARP response faker element
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

#include <click/config.h>
#include "arpfaker.hh"
#include "arpresponder.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

ARPFaker::ARPFaker()
  : _timer(this)
{
}

ARPFaker::~ARPFaker()
{
}

int
ARPFaker::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read_mp("DSTIP", _ip1)
	.read_mp("DSTETH", _eth1)
	.read_mp("SRCIP", _ip2)
	.read_mp("SRCETH", _eth2)
	.complete();
}

int
ARPFaker::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_msec(1 * 1000); // Send an ARP reply periodically.
    return 0;
}

void
ARPFaker::run_timer(Timer *)
{
    if (Packet *p = ARPResponder::make_response(_eth1.data(), _ip1.data(),
						_eth2.data(), _ip2.data()))
	output(0).push(p);
    _timer.schedule_after_msec(10 * 1000);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ARPResponder)
EXPORT_ELEMENT(ARPFaker)
