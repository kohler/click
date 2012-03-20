/*
 * discard.{cc,hh} -- element throws away all packets
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
#include "discard.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

Discard::Discard()
    : _task(this), _count(0), _burst(1), _active(true)
{
}

int
Discard::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	.read("ACTIVE", _active)
	.read("BURST", _burst)
	.complete() < 0)
	return -1;
    if (!_active && input_is_push(0))
	return errh->error("ACTIVE is meaningless in push context");
    if (_burst == 0)
	_burst = ~(unsigned) 0;
    return 0;
}

int
Discard::initialize(ErrorHandler *errh)
{
    if (input_is_pull(0)) {
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
	_signal = Notifier::upstream_empty_signal(this, 0, &_task);
    }
    return 0;
}

void
Discard::push(int, Packet *p)
{
    _count++;
    p->kill();
}

bool
Discard::run_task(Task *)
{
    unsigned x = _burst;
    Packet *p;
    while (x && (p = input(0).pull())) {
	p->kill();
	--x;
    }
    unsigned sent = _burst - x;

    _count += sent;
    if (_active && (sent || _signal))
	_task.fast_reschedule();
    return sent != 0;
}

int
Discard::write_handler(const String &s, Element *e, void *user_data,
		       ErrorHandler *errh)
{
    Discard *d = static_cast<Discard *>(e);
    if (!user_data)
	d->_count = 0;
    else {
	if (!BoolArg().parse(s, d->_active))
	    return errh->error("syntax error");
	if (d->_active)
	    d->_task.reschedule();
	else
	    d->_task.unschedule();
    }
    return 0;
}

void
Discard::add_handlers()
{
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_write_handler("reset_counts", write_handler, h_reset_counts, Handler::BUTTON);
    if (input_is_pull(0)) {
	add_data_handlers("active", Handler::OP_READ | Handler::CHECKBOX, &_active);
	add_write_handler("active", write_handler, h_active);
	add_task_handlers(&_task);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Discard)
ELEMENT_MT_SAFE(Discard)
