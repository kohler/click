/*
 * getetheraddress.{cc,hh} -- writes an ethernet address from anno to packet data
 * Cliff Frey
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2008-2012 Meraki, Inc.
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
#include "getetheraddress.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

int
GetEtherAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int anno;
    uint32_t offset;
    String off;
    if (Args(conf, this, errh)
	.read_mp("ANNO", AnnoArg(6), anno)
	.read_mp("OFFSET", WordArg(), off)
	.complete() < 0)
	return -1;

    if (off.lower() == "src")
	offset = 6;
    else if (off.lower() == "dst")
	offset = 0;
    else if (!IntArg().parse(off, offset) || offset + 6 < 6)
	return errh->error("type mismatch: bad OFFSET");

    _anno = anno;
    _offset = offset;
    return 0;
}

Packet *
GetEtherAddress::simple_action(Packet *p)
{
    if (_offset + 6 <= p->length()) {
	memcpy(p->anno_u8() + _anno, p->data() + _offset, 6);
	return p;
    } else {
	checked_output_push(1, p);
	return 0;
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GetEtherAddress)
ELEMENT_MT_SAFE(GetEtherAddress)
