// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * truncate.{cc,hh} -- element strips bytes from back of packet
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
#include "truncate.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

Truncate::Truncate()
    : Element(1, 1)
{
}

Truncate::~Truncate()
{
}

int
Truncate::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return cp_va_parse(conf, this, errh,
		       cpUnsigned, "number of bytes to strip", &_nbytes,
		       cpEnd);
}

Packet *
Truncate::simple_action(Packet *p)
{
    p->take(_nbytes);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Truncate)
ELEMENT_MT_SAFE(Truncate)
