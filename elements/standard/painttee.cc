/*
 * painttee.{cc,hh} -- element checks paint annotation
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "painttee.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

PaintTee::PaintTee()
{
}

int
PaintTee::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int anno = PAINT_ANNO_OFFSET;
    if (Args(conf, this, errh)
	.read_mp("COLOR", _color)
	.read_p("ANNO", AnnoArg(1), anno).complete() < 0)
	return -1;
    _anno = anno;
    return 0;
}

Packet *
PaintTee::simple_action(Packet *p)
{
    if (p->anno_u8(_anno) == _color)
	output(1).push(p->clone());
    return(p);
}

void
PaintTee::add_handlers()
{
    add_data_handlers("color", Handler::OP_READ | Handler::OP_WRITE, &_color);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PaintTee)
ELEMENT_MT_SAFE(PaintTee)
