/*
 * rrunqueue.{cc,hh} -- element pulls as many packets as possible from its
 * inputs, pushes them out its outputs
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/error.hh>
#include "rrunqueue.hh"
#include <click/confparse.hh>
#include "elements/standard/scheduleinfo.hh"

RoundRobinUnqueue::RoundRobinUnqueue()
  : _task(this), _next(0)
{
  MOD_INC_USE_COUNT;
}

RoundRobinUnqueue::~RoundRobinUnqueue()
{
  MOD_DEC_USE_COUNT;
}

int
RoundRobinUnqueue::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _burst = 1;
  return cp_va_parse(conf, this, errh,
		     cpOptional,
		     cpUnsigned, "burst size", &_burst,
		     0);
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

void
RoundRobinUnqueue::uninitialize()
{
  _task.unschedule();
}

void
RoundRobinUnqueue::run_scheduled()
{
  int sent = 0;
  Packet *p_next = input(_next).pull();
  
  while (p_next) {
    Packet *p = p_next;
    sent++;
    if (sent < _burst || _burst == 0) {
      p_next = input(_next).pull();
    }
    else 
      p_next = 0;
#ifdef __KERNEL__
#if __i386__ && HAVE_INTEL_CPU
    if (p_next) {
      struct sk_buff *skb = p_next->steal_skb();
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
  add_read_handler("packets", read_param, (void *)0);
  add_task_handlers(&_task);
}

EXPORT_ELEMENT(RoundRobinUnqueue)
ELEMENT_MT_SAFE(RoundRobinUnqueue)
