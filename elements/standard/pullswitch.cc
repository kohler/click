/*
 * pullswitch.{cc,hh} -- element routes packets from one input of several
 * Eddie Kohler
 *
 * Copyright (c) 2009 Intel Corporation
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
#include "pullswitch.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/llrpc.h>
CLICK_DECLS

PullSwitch::PullSwitch()
    : _notifier(Notifier::SEARCH_CONTINUE_WAKE), _signals(0)
{
}

PullSwitch::~PullSwitch()
{
}

void *
PullSwitch::cast(const char *name)
{
    if (strcmp(name, "SimplePullSwitch") == 0)
	return static_cast<SimplePullSwitch *>(this);
    else if (strcmp(name, Notifier::EMPTY_NOTIFIER) == 0)
	return static_cast<Notifier *>(&_notifier);
    else
	return Element::cast(name);
}

int
PullSwitch::initialize(ErrorHandler *errh)
{
    _notifier.initialize(Notifier::EMPTY_NOTIFIER, router());
    _notifier.set_active(_input >= 0, false);
    if (!(_signals = new NotifierSignal[ninputs()]))
	return errh->error("out of memory!");
    for (int i = 0; i < ninputs(); ++i)
	_signals[i] = Notifier::upstream_empty_signal(this, i, &_notifier);
    return 0;
}

void
PullSwitch::cleanup(CleanupStage)
{
    delete[] _signals;
    _signals = 0;
}

Packet *
PullSwitch::pull(int)
{
    if (_input < 0)
	return 0;
    else if (Packet *p = input(_input).pull()) {
	_notifier.set_active(true, false);
	return p;
    } else {
	if (!_signals[_input])
	    _notifier.set_active(false, false);
	return 0;
    }
}

void
PullSwitch::set_input(int input)
{
    _input = (input < 0 || input >= ninputs() ? -1 : input);
    if (!_notifier.initialized())
	/* do nothing, this is the set_input() called from configure() */;
    else if (_input < 0)
	_notifier.set_active(false, false);
    else if (_signals[_input])
	_notifier.set_active(true, true);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(SimplePullSwitch)
EXPORT_ELEMENT(PullSwitch)
ELEMENT_MT_SAFE(PullSwitch)
