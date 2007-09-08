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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

Unqueue::Unqueue()
    : _task(this)
{
}

Unqueue::~Unqueue()
{
}

int
Unqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 1;
    _active = true;
    return cp_va_kparse(conf, this, errh,
			"BURST", cpkP, cpInteger, &_burst,
			"ACTIVE", 0, cpBool, &_active,
			cpEnd);
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

    int worked = 0;
    while (worked < _burst) {
	if (Packet *p = input(0).pull()) {
	    worked++;
	    output(0).push(p);
	} else {
	    if (!_signal) {
		_count += worked;
		return worked > 0;
	    }
	    break;
	}
    }
    
    _task.fast_reschedule();
    _count += worked;
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

enum { H_COUNT, H_ACTIVE };

String
Unqueue::read_param(Element *e, void *thunk)
{
    Unqueue *u = (Unqueue *)e;
    switch ((uintptr_t) thunk) {
      case H_COUNT:
	return String(u->_count);
      case H_ACTIVE:
	return String(u->_active);
      default:
	return "<error>";
    }
}

int 
Unqueue::write_param(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
    Unqueue *u = (Unqueue *)e;
    String s = cp_uncomment(conf);
    switch ((uintptr_t) thunk) {
      case H_ACTIVE:		// active
	if (!cp_bool(s, &u->_active))
	    return errh->error("active parameter must be boolean");    
	if (u->_active && !u->_task.scheduled())
	    u->_task.reschedule();
	break;
    }
    return 0;
}

void
Unqueue::add_handlers()
{
    add_read_handler("count", read_param, (void *)H_COUNT);
    add_read_handler("active", read_param, (void *)H_ACTIVE);
    add_write_handler("active", write_param, (void *)H_ACTIVE);
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Unqueue)
ELEMENT_MT_SAFE(Unqueue)
