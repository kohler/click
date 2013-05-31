/*
 * ipclassifier.{cc,hh} -- IP-packet classifier with tcpdumplike syntax
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2010 Meraki, Inc.
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
#include "ipclassifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/router.hh>
CLICK_DECLS

IPClassifier::IPClassifier()
{
}

IPClassifier::~IPClassifier()
{
}

int
IPClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() != noutputs())
	return errh->error("need %d arguments, one per output port", noutputs());

    // leverage IPFilter's parsing
    Vector<String> new_conf;
    for (int i = 0; i < conf.size(); i++)
	new_conf.push_back(String(i) + " " + conf[i]);
    int r = IPFilter::configure(new_conf, errh);
    if (r >= 0 && !router()->initialized())
	_zprog.warn_unused_outputs(noutputs(), errh);
    return r;
}

void IPClassifier::add_handlers() {
    IPFilter::add_handlers();
    for (uintptr_t i = 0; i != (uintptr_t) noutputs(); ++i) {
	add_read_handler("pattern" + String(i), read_positional_handler, (void*) i);
	add_write_handler("pattern" + String(i), reconfigure_positional_handler, (void*) i);
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPFilter)
EXPORT_ELEMENT(IPClassifier)
ELEMENT_MT_SAFE(IPClassifier)
