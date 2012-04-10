/*
 * notifierdebug.{cc,hh} -- a "signal" handler unparses nearby signal
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "notifierdebug.hh"
CLICK_DECLS

NotifierDebug::NotifierDebug()
{
}

int
NotifierDebug::initialize(ErrorHandler *)
{
    if (input_is_push(0))
	_signal = Notifier::downstream_full_signal(this, 0);
    else
	_signal = Notifier::upstream_empty_signal(this, 0);
    return 0;
}

Packet *
NotifierDebug::simple_action(Packet *p)
{
    return p;
}

String
NotifierDebug::read_handler(Element *e, void *)
{
    NotifierDebug *nd = static_cast<NotifierDebug *>(e);
    return nd->_signal.unparse(nd->router());
}

void
NotifierDebug::add_handlers()
{
    add_read_handler("signal", read_handler);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NotifierDebug)
