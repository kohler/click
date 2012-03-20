// -*- c-basic-offset: 4 -*-
/*
 * unqueue.{cc,hh} -- element pulls as many packets as possible from
 * its input, pushes them out its output
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2002 International Computer Science Institute
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
#include "unqueue.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

Unqueue::Unqueue()
    : _task(this)
{
}

int
Unqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 1;
    _limit = -1;
    _active = true;
    return Args(conf, this, errh)
	.read_p("BURST", _burst)
	.read("ACTIVE", _active)
	.read("LIMIT", _limit).complete();
}

int
Unqueue::initialize(ErrorHandler *errh)
{
    _count = 0;
    ScheduleInfo::initialize_task(this, &_task, _active, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    if (_burst < 0)
	_burst = 0x7FFFFFFFU;
    else if (_burst == 0)
	errh->warning("BURST size 0, no packets will be pulled");
    return 0;
}

bool
Unqueue::run_task(Task *)
{
    if (!_active)
	return false;

    int worked = 0, limit = _burst;
    if (_limit >= 0 && _count + limit >= (uint32_t) _limit) {
	limit = _limit - _count;
	if (limit <= 0)
	    return false;
    }

    while (worked < limit && _active) {
	if (Packet *p = input(0).pull()) {
	    ++worked;
	    ++_count;
	    output(0).push(p);
	} else if (!_signal)
	    goto out;
	else
	    break;
    }

    _task.fast_reschedule();
  out:
    return worked > 0;
}

#if 0 && defined(CLICK_LINUXMODULE)
#if __i386__ && HAVE_INTEL_CPU
/* Old prefetching code from run_task(). */
  if (p_next) {
    struct sk_buff *skb = p_next->skb();
    asm volatile("prefetcht0 %0" : : "m" (skb->len));
    asm volatile("prefetcht0 %0" : : "m" (skb->cb[0]));
  }
#endif
#endif

int
Unqueue::write_param(const String &conf, Element *e, void *user_data,
		     ErrorHandler *errh)
{
    Unqueue *u = static_cast<Unqueue *>(e);
    switch (reinterpret_cast<intptr_t>(user_data)) {
    case h_active:
	if (!BoolArg().parse(conf, u->_active))
	    return errh->error("syntax error");
	break;
    case h_reset:
	u->_count = 0;
	break;
    case h_limit:
	if (!IntArg().parse(conf, u->_limit))
	    return errh->error("syntax error");
	break;
    case h_burst:
	if (!IntArg().parse(conf, u->_burst))
	    return errh->error("syntax error");
	if (u->_burst < 0)
	    u->_burst = 0x7FFFFFFF;
	break;
    }
    if (u->_active && !u->_task.scheduled()
	&& (u->_limit < 0 || u->_count < (uint32_t) u->_limit))
	u->_task.reschedule();
    return 0;
}

void
Unqueue::add_handlers()
{
    add_data_handlers("active", Handler::OP_READ | Handler::CHECKBOX, &_active);
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_data_handlers("burst", Handler::OP_READ, &_burst);
    add_data_handlers("limit", Handler::OP_READ, &_limit);
    add_write_handler("active", write_param, h_active);
    add_write_handler("reset", write_param, h_reset, Handler::BUTTON);
    add_write_handler("reset_counts", write_param, h_reset, Handler::BUTTON | Handler::UNCOMMON);
    add_write_handler("burst", write_param, h_burst);
    add_write_handler("limit", write_param, h_limit);
    add_task_handlers(&_task, &_signal);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Unqueue)
ELEMENT_MT_SAFE(Unqueue)
