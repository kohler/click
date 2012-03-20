/*
 * paintswitch.{cc,hh} -- element routes packets to one output of several
 * Douglas S. J. De Couto.  Based on Switch element by Eddie Kohler
 *
 * Copyright (c) 2002 MIT
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
#include "paintswitch.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

PaintSwitch::PaintSwitch()
{
}

int
PaintSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int anno = PAINT_ANNO_OFFSET;
    if (Args(conf, this, errh).read_p("ANNO", AnnoArg(1), anno).complete() < 0)
	return -1;
    _anno = anno;
    return 0;
}

void
PaintSwitch::push(int, Packet *p)
{
    int output_port = static_cast<int>(p->anno_u8(_anno));
    if (output_port != 0xFF)
	checked_output_push(output_port, p);
    else { // duplicate to all output ports
	int n = noutputs();
	for (int i = 0; i < n - 1; i++)
	    if (Packet *q = p->clone())
		output(i).push(q);
	output(n - 1).push(p);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PaintSwitch)
ELEMENT_MT_SAFE(PaintSwitch)
