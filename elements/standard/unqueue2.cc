/*
 * unqueue2.{cc,hh} -- element pulls as many packets as possible from its
 * input, pushes them out its output. don't pull if queues downstream are
 * full.
 * Eddie Kohler, Benjie Chen
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
#include <click/error.hh>
#include <click/router.hh>
#include <click/routervisitor.hh>
#include <click/standard/storage.hh>
#include "unqueue2.hh"
#include <click/args.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

Unqueue2::Unqueue2()
    : _burst(1), _count(0), _task(this)
{
}

int
Unqueue2::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String queues_string;
    bool quiet = false;
    if (Args(conf, this, errh)
	.read_p("BURST", _burst)
	.read("QUEUES", AnyArg(), queues_string).read_status(_explicit_queues)
	.read("QUIET", quiet)
	.complete() < 0)
	return -1;
    while (String word = cp_shift_spacevec(queues_string)) {
	_queues.push_back(0);
	if (!ElementCastArg("Storage").parse(word, _queues.back(), this))
	    return errh->error("bad QUEUES");
    }
    if (_burst == 0)
	_burst = INT_MAX;
    if (!quiet)
	errh->error("Unqueue2 is deprecated, you should probably use Unqueue");
    return 0;
}

int
Unqueue2::initialize(ErrorHandler *errh)
{
    if (!_explicit_queues) {
	ElementCastTracker filter(router(), "Storage");
	if (router()->visit_downstream(this, 0, &filter) < 0)
	    return errh->error("flow-based router context failure");
	for (Element * const *it = filter.begin(); it != filter.end(); ++it)
	    _queues.push_back((Storage *) (*it)->cast("Storage"));
    }
    ScheduleInfo::initialize_task(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    return 0;
}

bool
Unqueue2::run_task(Task *)
{
    int worked = 0, limit = _burst;
    for (Storage **it = _queues.begin(); it != _queues.end(); ++it) {
	int space = (*it)->capacity() - (*it)->size();
	if (limit < 0 || space < limit)
	    limit = space;
    }

    while (worked < limit) {
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

#if CLICK_LINUXMODULE && __i386__ && HAVE_INTEL_CPU && 0
	if (p_next) {
	    struct sk_buff *skb = p_next->skb();
	    asm volatile("prefetcht0 %0" : : "m" (skb->len));
	    asm volatile("prefetcht0 %0" : : "m" (skb->cb[0]));
	}
#endif

String
Unqueue2::read_param(Element *e, void *)
{
    Unqueue2 *u = (Unqueue2 *)e;
    return String(u->_count) + " packets\n";
}

void
Unqueue2::add_handlers()
{
  add_read_handler("packets", read_param, 0);
  add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Unqueue2)
ELEMENT_MT_SAFE(Unqueue2)
