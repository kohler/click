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
#include "priosched2.hh"
#include <click/error.hh>
CLICK_DECLS

PrioSched2::PrioSched2()
    : Element(0, 1), _signals(0)
{
    MOD_INC_USE_COUNT;
}

PrioSched2::~PrioSched2()
{
    MOD_DEC_USE_COUNT;
}

#if 0
Notifier::SearchOp
PrioSched2::notifier_search_op()
{
    return SEARCH_UPSTREAM_LISTENERS;
}
void *
PrioSched2::cast(const char *n)
{
    if (strcmp(n, "Notifier") == 0)
	return static_cast<Notifier *>(this);
    else
	return Element::cast(n);
}
#endif

void
PrioSched2::notify_ninputs(int n)
{
    set_ninputs(n);
}

int 
PrioSched2::initialize(ErrorHandler *errh)
{
    if (!(_signals = new NotifierSignal[ninputs()]))
	return errh->error("out of memory!");
    for (int i = 0; i < ninputs(); i++)
	_signals[i] = Notifier::upstream_pull_signal(this, i, 0);
    return 0;
}

void
PrioSched2::cleanup(CleanupStage)
{
    delete[] _signals;
}

Packet *
PrioSched2::pull(int)
{
    Packet *p;
    for (int i = 0; i < ninputs(); i++)
	if (_signals[i] && (p = input(i).pull()))
	    return p;
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrioSched2)
ELEMENT_MT_SAFE(PrioSched2)
