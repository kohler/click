// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * netmapinfo.{cc,hh} -- library for interfacing with netmap
 *
 * Copyright (c) 2015 Tom Barbette
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
#include <click/args.hh>
#include <click/error.hh>
#include "netmapinfo.hh"

CLICK_DECLS

int NetmapInfo::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (instance) {
		return errh->error("You cannot place multiple instances of NetmapInfo !");
	}
	instance = this;
	if (Args(conf, this, errh)
			.read_p("EXTRA_BUFFER", NetmapDevice::global_alloc)
			.complete() < 0)
		return -1;

	return 0;
}

NetmapInfo* NetmapInfo::instance = 0;

CLICK_ENDDECLS

ELEMENT_REQUIRES(userlevel netmap)
EXPORT_ELEMENT(NetmapInfo)
ELEMENT_MT_SAFE(NetmapInfo)
