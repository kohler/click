/*
 * delayunqueue.{cc,hh} -- element pulls packets from input, delays pushing
 * the packet to output port.
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include <click/package.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include "delayunqueue.hh"
#include <click/standard/scheduleinfo.hh>

DelayUnqueue::DelayUnqueue()
  : Element(1, 1), _p(0), _task(this), _timer(&_task)
{
  MOD_INC_USE_COUNT;
}

DelayUnqueue::~DelayUnqueue()
{
  MOD_DEC_USE_COUNT;
}

int
DelayUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpInterval, "delay", &_delay, 0);
}

int
DelayUnqueue::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, &_task, errh);
  
  // initialize Timer if we have a long delay (>= 0.1sec)
  if (_delay.tv_sec > 0 || _delay.tv_usec >= 100000)
    _timer.initialize(this);
  
  _signal = ActivityNotifier::listen_upstream_pull(this, 0, &_task);
  return 0;
}

void
DelayUnqueue::uninitialize()
{
  if (_p) {
    _p->kill();
    _p = 0;
  }
}

void
DelayUnqueue::run_scheduled()
{
  if (!_p && (_p = input(0).pull())) {
    timeradd(&_p->timestamp_anno(), &_delay, &_p->timestamp_anno());
    if (_timer.initialized()) {	// long delay, use timer
      _timer.schedule_at(_p->timestamp_anno());
      return;			// without rescheduling
    }
  }
  
  if (_p) {
    struct timeval now;
    click_gettimeofday(&now);
    if (!timercmp(&now, &_p->timestamp_anno(), <)) {
      _p->timestamp_anno() = now;
      output(0).push(_p);
      _p = 0;
    }
  } else if (!_signal.active())
    return;			// without rescheduling

  _task.fast_reschedule();
}

String
DelayUnqueue::read_param(Element *e, void *)
{
  DelayUnqueue *u = (DelayUnqueue *)e;
  return cp_unparse_interval(u->_delay) + "\n";
}

void
DelayUnqueue::add_handlers()
{
  add_read_handler("delay", read_param, (void *)0);
  add_task_handlers(&_task);
}

EXPORT_ELEMENT(DelayUnqueue)
ELEMENT_MT_SAFE(DelayUnqueue)
