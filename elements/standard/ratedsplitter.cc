// -*- c-basic-offset: 4 -*-
/*
 * ratedsplitter.{cc,hh} -- split packets at a given rate.
 * Benjie Chen, Eddie Kohler
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
#include "ratedsplitter.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/args.hh>
#include "ratedunqueue.hh"
CLICK_DECLS

RatedSplitter::RatedSplitter()
{
}

int
RatedSplitter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return RatedUnqueue::configure_helper(&_tb, is_bandwidth(), this, conf, errh);
}

void
RatedSplitter::push(int, Packet *p)
{
    _tb.refill();
    if (_tb.remove_if(1))
	output(0).push(p);
    else
	checked_output_push(1, p);
}


// HANDLERS

String
RatedSplitter::read_handler(Element *e, void *)
{
    RatedSplitter *rs = static_cast<RatedSplitter *>(e);
    if (rs->is_bandwidth())
	return BandwidthArg::unparse(rs->_tb.rate());
    else
	return String(rs->_tb.rate());
}

void
RatedSplitter::add_handlers()
{
    add_read_handler("rate", read_handler, 0);
    add_write_handler("rate", reconfigure_keyword_handler, "0 RATE");
    add_read_handler("config", read_handler, 0);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RatedSplitter)
ELEMENT_REQUIRES(RatedUnqueue)
