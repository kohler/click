// -*- c-basic-offset: 4 -*-
/*
 * priosched.{cc,hh} -- priority scheduler element
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/error.hh>
#include "priosched.hh"
CLICK_DECLS

PrioSched::PrioSched()
    : _signals(0)
{
}

PrioSched::~PrioSched()
{
}

int
PrioSched::initialize(ErrorHandler *errh)
{
    if (!(_signals = new NotifierSignal[ninputs()]))
	return errh->error("out of memory!");
    for (int i = 0; i < ninputs(); i++)
	_signals[i] = Notifier::upstream_empty_signal(this, i);
    return 0;
}

void
PrioSched::cleanup(CleanupStage)
{
    delete[] _signals;
}

Packet *
PrioSched::pull(int)
{
    Packet *p;
    for (int i = 0; i < ninputs(); i++)
	if (_signals[i] && (p = input(i).pull()))
	    return p;
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrioSched)
ELEMENT_MT_SAFE(PrioSched)
