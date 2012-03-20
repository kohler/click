// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * strip.{cc,hh} -- element strips bytes from front of packet
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "strip.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

Strip::Strip()
{
}

int
Strip::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("LENGTH", _nbytes).complete();
}

Packet *
Strip::simple_action(Packet *p)
{
    p->pull(_nbytes);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Strip)
ELEMENT_MT_SAFE(Strip)
