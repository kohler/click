// -*- c-basic-offset: 4 -*-
/*
 * blockthread.{cc,hh} -- test element that blocks execution
 * Eddie Kohler
 *
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
#include "blockthread.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <sys/select.h>
CLICK_DECLS

BlockThread::BlockThread()
{
}

int
BlockThread::handler(int, String &str, Element *, const Handler *, ErrorHandler *errh)
{
    struct timeval tv;
    if (!cp_time(str, &tv))
	return errh->error("bad TIME");
    int r = select(0, 0, 0, 0, &tv);
    str = String(r);
    return 0;
}

void
BlockThread::add_handlers()
{
    set_handler("block", Handler::h_write | Handler::h_write_private, handler, 0, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BlockThread)
ELEMENT_REQUIRES(userlevel)
