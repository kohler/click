// -*- c-basic-offset: 4 -*-
/*
 * pad.{cc,hh} -- 
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
CLICK_DECLS

Pad::Pad()
    : Element(1, 1)
{
}

Pad::~Pad()
{
}

Packet *
Pad::simple_action(Packet *p_in)
{
    if (EXTRA_LENGTH_ANNO(p_in)) {
	if (WritablePacket *p = p_in->put(EXTRA_LENGTH_ANNO(p_in))) {
	    SET_EXTRA_LENGTH_ANNO(p, 0);
	    return p;
	} else
	    return 0;
    } else
	return p_in;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Pad)
ELEMENT_MT_SAFE(Pad)

