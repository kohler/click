// -*- c-basic-offset: 4 -*-
/*
 * cyclecountaccum.{cc,hh} -- accumulate cycle counter deltas
 * Eddie Kohler
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
#include "cyclecountaccum.hh"
#include <click/packet_anno.hh>
#include <click/glue.hh>

CycleCountAccum::CycleCountAccum()
    : _accum(0), _count(0), _zero_count(0)
{
}

CycleCountAccum::~CycleCountAccum()
{
}

inline void
CycleCountAccum::smaction(Packet *p)
{
    if (PERFCTR_ANNO(p)) {
	_accum += click_get_cycles() - PERFCTR_ANNO(p);
	_count++;
    } else {
	_zero_count++;
	if (_zero_count == 1)
	    click_chatter("%s: packet with zero cycle counter annotation!", declaration().c_str());
    }
}

void
CycleCountAccum::push(int, Packet *p)
{
    smaction(p);
    output(0).push(p);
}

Packet *
CycleCountAccum::pull(int)
{
    Packet *p = input(0).pull();
    if (p)
	smaction(p);
    return p;
}

String
CycleCountAccum::read_handler(Element *e, void *thunk)
{
    CycleCountAccum *cca = static_cast<CycleCountAccum *>(e);
    switch ((uintptr_t)thunk) {
      case 0:
	return String(cca->_count);
      case 1:
	return String(cca->_accum);
      case 2:
	return String(cca->_zero_count);
      default:
	return String();
    }
}

int
CycleCountAccum::reset_handler(const String &, Element *e, void *, ErrorHandler *)
{
    CycleCountAccum *cca = static_cast<CycleCountAccum *>(e);
    cca->_count = cca->_accum = cca->_zero_count = 0;
    return 0;
}

void
CycleCountAccum::add_handlers()
{
    add_read_handler("count", read_handler, 0);
    add_read_handler("cycles", read_handler, 1);
    add_read_handler("zero_count", read_handler, 2);
    add_write_handler("reset_counts", reset_handler, 0, Handler::BUTTON);
}

ELEMENT_REQUIRES(linuxmodule int64)
EXPORT_ELEMENT(CycleCountAccum)
