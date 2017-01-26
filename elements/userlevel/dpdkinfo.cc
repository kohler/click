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

int DPDKInfo::configure(Vector<String> &conf, ErrorHandler *errh) {
    if (instance) {
        return errh->error(
            "There can be only one instance of DPDKInfo!");
    }
    instance = this;
    if (Args(conf, this, errh)
        .read_p("NB_MBUF", DPDKDevice::NB_MBUF)
        .read("MBUF_SIZE", DPDKDevice::MBUF_SIZE)
        .read("MBUF_CACHE_SIZE", DPDKDevice::MBUF_CACHE_SIZE)
        .read("RX_PTHRESH", DPDKDevice::RX_PTHRESH)
        .read("RX_HTHRESH", DPDKDevice::RX_HTHRESH)
        .read("RX_WTHRESH", DPDKDevice::RX_WTHRESH)
        .read("TX_PTHRESH", DPDKDevice::TX_PTHRESH)
        .read("TX_HTHRESH", DPDKDevice::TX_HTHRESH)
        .read("TX_WTHRESH", DPDKDevice::TX_WTHRESH)
        .complete() < 0)
        return -1;

    return 0;
}

DPDKInfo* DPDKInfo::instance = 0;

CLICK_ENDDECLS

ELEMENT_REQUIRES(dpdk)
EXPORT_ELEMENT(DPDKInfo)
