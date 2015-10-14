// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * dpdkinfo.{cc,hh} -- library for interfacing with dpdk
 *
 * Copyright (c) 2014-2015 Cyril Soldani, University of Liège
 * Copyright (c) 2015 Tom Barbette, University of Liège
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
#include "dpdkinfo.hh"

CLICK_DECLS

int DpdkInfo::configure(Vector<String> &conf, ErrorHandler *errh) {
    if (instance) {
        return errh->error(
            "There can be only one instance of DpdkInfo!");
    }
    instance = this;
    if (Args(conf, this, errh)
        .read_p("NB_MBUF", DpdkDevice::NB_MBUF)
        .read_p("MBUF_SIZE", DpdkDevice::MBUF_SIZE)
        .read_p("MBUF_CACHE_SIZE", DpdkDevice::MBUF_CACHE_SIZE)
        .read_p("RX_PTHRESH", DpdkDevice::RX_PTHRESH)
        .read_p("RX_HTHRESH", DpdkDevice::RX_HTHRESH)
        .read_p("RX_WTHRESH", DpdkDevice::RX_WTHRESH)
        .read_p("TX_PTHRESH", DpdkDevice::TX_PTHRESH)
        .read_p("TX_HTHRESH", DpdkDevice::TX_HTHRESH)
        .read_p("TX_WTHRESH", DpdkDevice::TX_WTHRESH)
        .complete() < 0)
        return -1;

    return 0;
}

DpdkInfo* DpdkInfo::instance = 0;

CLICK_ENDDECLS

ELEMENT_REQUIRES(dpdk)
EXPORT_ELEMENT(DpdkInfo)
