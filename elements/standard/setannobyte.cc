/*
 * setannobyte.{cc,hh} -- element sets packets' user annotation
 * John Bicket
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
#include "setannobyte.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
CLICK_DECLS

SetAnnoByte::SetAnnoByte()
  : _offset(0), _value(0)
{
}

int
SetAnnoByte::configure(Vector<String> &conf, ErrorHandler *errh)
{
    errh->warning("SetAnnoByte(ANNO, VALUE) is obsolete, use Paint(VALUE, ANNO) instead");
    return Args(conf, this, errh)
	.read_mp("ANNO", AnnoArg(1), _offset)
	.read_mp("VALUE", _value).complete();
}

Packet *
SetAnnoByte::simple_action(Packet *p)
{
    p->set_anno_u8(_offset, _value);
    return p;
}

void
SetAnnoByte::add_handlers()
{
    add_data_handlers("anno", Handler::OP_READ, &_offset);
    add_data_handlers("value", Handler::OP_READ | Handler::OP_WRITE, &_value);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetAnnoByte)
ELEMENT_MT_SAFE(SetAnnoByte)
