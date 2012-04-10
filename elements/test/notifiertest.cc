/*
 * notifiertest.{cc,hh} -- test Notifier functionality
 * Eddie Kohler
 *
 * Copyright (c) 2012 Eddie Kohler
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
#include "notifiertest.hh"
CLICK_DECLS

NotifierTest::NotifierTest()
{
}

#define CHECK(x) do {							\
	if (!(x))							\
	    return errh->error("%s:%d: test %<%s%> failed", __FILE__, __LINE__, #x); \
    } while (0)

int
NotifierTest::initialize(ErrorHandler *errh)
{
    CHECK(NotifierSignal::busy_signal() + NotifierSignal::overderived_signal() == NotifierSignal::busy_signal());
    CHECK(NotifierSignal::overderived_signal() + NotifierSignal::busy_signal() == NotifierSignal::busy_signal());
    errh->message("All tests pass!");
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NotifierTest)
