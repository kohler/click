// -*- c-basic-offset: 4 -*-
/*
 * flowinfo.{cc,hh} -- element allows overriding flow codes
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
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
#include "flowinfo.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

FlowInfo::FlowInfo()
{
}

int
FlowInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    for (String *it = conf.begin(); it != conf.end(); ++it) {
	String name = cp_shift_spacevec(*it);
	String code = cp_shift_spacevec(*it);
	Element *e;
	if (!name || !code || *it)
	    errh->error("expected %<ELEMENT CODE%>");
	else if ((e = cp_element(name, this, errh)))
	    router()->set_flow_code_override(e->eindex(), code);
    }
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FlowInfo)
