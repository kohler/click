/*
 * unqueue.{cc,hh} -- element pulls as many packets as possible from
 * its input, pushes them out its output
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
#include "unqueue.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include "elements/standard/scheduleinfo.hh"

Unqueue::Unqueue()
  : Element(1, 1), _task(this)
{
  MOD_INC_USE_COUNT;
}

Unqueue::~Unqueue()
{
  MOD_DEC_USE_COUNT;
}

int
Unqueue::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _burst = 1;
  _active = true;
  return cp_va_parse(conf, this, errh,
		     cpOptional,
		     cpUnsigned, "burst size", &_burst,
		     cpKeywords,
		     "ACTIVE", cpBool, "is active?", &_active,
		     0);
}

int
Unqueue::initialize(ErrorHandler *errh)
{
  _count = 0;
  if (_active)
    ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
Unqueue::uninitialize()
{
  _task.unschedule();
}

void
Unqueue::run_scheduled()
{
  if (!_active)
    return;

  int sent = 0;
  Packet *p_next = input(0).pull();
  
  while (_active && p_next) {
    Packet *p = p_next;
    sent++;
    if (sent < _burst || _burst == 0) {
      p_next = input(0).pull();
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
    output(0).push(p);
    _count++;
  }

  if (_active)
    _task.fast_reschedule();
}

String
Unqueue::read_param(Element *e, void *)
{
  Unqueue *u = (Unqueue *)e;
  return String(u->_count) + "\n";
}

int 
Unqueue::write_param(const String &conf, Element *e, 
		     void *thunk, ErrorHandler *errh)
{
  Unqueue *uq = (Unqueue *)e;
  String s = cp_uncomment(conf);
  switch ((int)thunk) {
    
   case 1: {			// active
     if (!cp_bool(s, &uq->_active))
       return errh->error("active parameter must be boolean");    
     if (uq->_active && !uq->_task.scheduled())
       uq->_task.reschedule();
     break;
   }
   
  }
  return 0;
}

void
Unqueue::add_handlers()
{
  add_read_handler("count", read_param, (void *)0);
  add_write_handler("active", write_param, (void *)1);
  add_task_handlers(&_task);
}

EXPORT_ELEMENT(Unqueue)
ELEMENT_MT_SAFE(Unqueue)
