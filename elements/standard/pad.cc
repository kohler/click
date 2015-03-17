// -*- c-basic-offset: 4 -*-
/*
 * pad.{cc,hh} -- extends packet length
 * Eddie Kohler
 *
 * Copyright (c) 2004 Regents of the University of California
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
#include "pad.hh"
#include <click/packet_anno.hh>
#include <click/args.hh>
CLICK_DECLS

Pad::Pad()
{
}

int
Pad::configure(Vector<String>& conf, ErrorHandler* errh)
{
    _nbytes = 0;
    _zero = true;
    return Args(conf, this, errh)
        .read_p("LENGTH", _nbytes)
        .read("ZERO", _zero)
        .complete();
}

Packet*
Pad::simple_action(Packet* p)
{
    uint32_t nput;
    if (unlikely(_nbytes))
        nput = p->length() < _nbytes ? _nbytes - p->length() : 0;
    else
        nput = EXTRA_LENGTH_ANNO(p);
    if (nput) {
        WritablePacket* q = p->put(nput);
        if (!q)
            return 0;
        if (_zero)
            memset(q->end_data() - nput, 0, nput);
        p = q;
    }
    SET_EXTRA_LENGTH_ANNO(p, 0);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Pad)
ELEMENT_MT_SAFE(Pad)

