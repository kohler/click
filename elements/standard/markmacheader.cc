/*
 * markmacheader.{cc,hh} -- element sets IP Header annotation
 * Eddie Kohler, Cliff Frey
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2009 Meraki, Inc.
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
#include "markmacheader.hh"
#include <click/args.hh>
CLICK_DECLS

MarkMACHeader::MarkMACHeader()
{
}

int
MarkMACHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t offset = 0, length = 0;
    if (Args(conf, this, errh)
	.read_p("OFFSET", offset)
	.read_p("LENGTH", length).complete() < 0)
	return -1;
    _offset = offset;
    _length = length;
    return 0;
}

Packet *
MarkMACHeader::simple_action(Packet *p)
{
    if (_length)
	p->set_mac_header(p->data() + _offset, _length);
    else
	p->set_mac_header(p->data() + _offset);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MarkMACHeader)
ELEMENT_MT_SAFE(MarkMACHeader)
