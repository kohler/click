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
  : _task(this)
{
}

Unqueue2::~Unqueue2()
{
}

int
Unqueue2::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _burst = 1;
    return Args(conf, this, errh).read_p("BURST", _burst).complete();
}

int
Unqueue2::initialize(ErrorHandler *errh)
{
  ElementCastTracker filter(router(), "Storage");
  if (router()->visit_downstream(this, 0, &filter) < 0)
    return errh->error("flow-based router context failure");
  _queue_elements = filter.elements();
  click_chatter("Unqueue2: found %d downstream queues", _queue_elements.size());
  _packets = 0;
  ScheduleInfo::initialize_task(this, &_task, errh);
  return 0;
}

bool
Unqueue2::run_task(Task *)
{
  int burst = -1;
  for (int i=0; i<_queue_elements.size(); i++) {
    Storage *s = (Storage*)_queue_elements[i]->cast("Storage");
    if (s) {
      int size = s->capacity()-s->size();
      if (burst < 0 || size < burst)
	burst = size;
    }
  }
  if (burst > _burst) burst = _burst;
  else if (burst == 0) {
    _task.fast_reschedule();
    return false;
  }

  int sent = 0;
  Packet *p_next = input(0).pull();

  while (p_next) {
    Packet *p = p_next;
    sent++;
    if (sent < burst || burst == 0) {
      p_next = input(0).pull();
    }
    else
      p_next = 0;
#ifdef CLICK_LINUXMODULE
#if __i386__ && HAVE_INTEL_CPU
    if (p_next) {
      struct sk_buff *skb = p_next->skb();
      asm volatile("prefetcht0 %0" : : "m" (skb->len));
      asm volatile("prefetcht0 %0" : : "m" (skb->cb[0]));
    }
#endif
#endif
    output(0).push(p);
    _packets++;
  }

  _task.fast_reschedule();
  return sent > 0;
}

String
Unqueue2::read_param(Element *e, void *)
{
  Unqueue2 *u = (Unqueue2 *)e;
  return String(u->_packets) + " packets\n";
}

void
Unqueue2::add_handlers()
{
  add_read_handler("packets", read_param, (void *)0);
  add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Unqueue2)
ELEMENT_MT_SAFE(Unqueue2)
