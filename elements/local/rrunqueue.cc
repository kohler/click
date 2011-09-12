/*
 * rrunqueue.{cc,hh} -- element pulls as many packets as possible from its
 * inputs, pushes them out its outputs
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
#include "rrunqueue.hh"
#include <click/args.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

RoundRobinUnqueue::RoundRobinUnqueue()
  : _task(this), _next(0)
{
}

RoundRobinUnqueue::~RoundRobinUnqueue()
{
}

int
RoundRobinUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _burst = 1;
  return Args(conf, this, errh)
      .read_p("BURST", _burst)
      .complete();
}

int
RoundRobinUnqueue::initialize(ErrorHandler *errh)
{
  if (noutputs() != ninputs()) {
    return errh->error("number of outputs must equal to number of inputs");
  }
  _packets = 0;
  ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

bool
RoundRobinUnqueue::run_task(Task *)
{
  int tries = 0;
  Packet *p_next = input(_next).pull();

  while (p_next) {
    Packet *p = p_next;
    tries++;
    if (tries < _burst || _burst == 0) {
      p_next = input(_next).pull();
    } else
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
    output(_next).push(p);
    _packets++;
  }

  if (_next == noutputs()-1)
    _next = 0;
  else
    _next++;
  _task.fast_reschedule();
  return true;
}

String
RoundRobinUnqueue::read_param(Element *e, void *)
{
  RoundRobinUnqueue *u = (RoundRobinUnqueue *)e;
  return String(u->_packets) + " packets\n";
}

void
RoundRobinUnqueue::add_handlers()
{
  add_read_handler("packets", read_param, 0);
  add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RoundRobinUnqueue)
ELEMENT_MT_SAFE(RoundRobinUnqueue)
