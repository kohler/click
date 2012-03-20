// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * shaper.{cc,hh} -- element limits number of successful pulls per
 * second to a given rate (packets/s)
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "shaper.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

Shaper::Shaper()
{
}

int
Shaper::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t rate;
    Args args(conf, this, errh);
    if (is_bandwidth())
	args.read_mp("RATE", BandwidthArg(), rate);
    else
	args.read_mp("RATE", rate);
    if (args.complete() < 0)
	return -1;
    _rate.set_rate(rate, errh);
    return 0;
}

Packet *
Shaper::pull(int)
{
    Packet *p = 0;
    if (_rate.need_update(Timestamp::now())) {
	if ((p = input(0).pull()))
	    _rate.update();
    }
    return p;
}

String
Shaper::read_handler(Element *e, void *)
{
    Shaper *s = static_cast<Shaper *>(e);
    if (s->is_bandwidth())
	return BandwidthArg::unparse(s->_rate.rate());
    else
	return String(s->_rate.rate());
}

void
Shaper::add_handlers()
{
    add_read_handler("rate", read_handler);
    add_write_handler("rate", reconfigure_keyword_handler, "0 RATE");
    add_read_handler("config", read_handler);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Shaper)
