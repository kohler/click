/*
 * delayunqueue.{cc,hh} -- element pulls packets from input, delays pushing
 * the packet to output port.
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "elements/standard/scheduleinfo.hh"

DelayUnqueue::DelayUnqueue()
  : Element(1,1), _task(this)
{
  MOD_INC_USE_COUNT;
}

DelayUnqueue::~DelayUnqueue()
{
  MOD_DEC_USE_COUNT;
}

int
DelayUnqueue::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpSecondsAsMicro, "delay (ms)", &_delay, 0);
}

int
DelayUnqueue::initialize(ErrorHandler *errh)
{
  _p = 0;
  ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
DelayUnqueue::uninitialize()
{
  if (_p) {
    _p->kill();
    _p = 0;
  }
  _task.unschedule();
}

static inline unsigned
elapsed_us(struct timeval tv)
{
  struct timeval t;
  unsigned e = 0;
  click_gettimeofday(&t);
  e = (t.tv_sec - tv.tv_sec)*1000000;
  if (t.tv_usec < tv.tv_usec) {
    t.tv_usec += 1000000;
    e -= 1000000;
  }
  e += t.tv_usec-tv.tv_usec;
  return e;
}

void
DelayUnqueue::run_scheduled()
{
  do {
    if (!_p) 
      _p = input(0).pull();
    if (!_p)
      break;
    unsigned t = elapsed_us(_p->timestamp_anno());
    if (t >= _delay) {
      output(0).push(_p);
      _p = 0;
    } 
  } while(!_p);

  _task.fast_reschedule();
}

String
DelayUnqueue::read_param(Element *e, void *)
{
  DelayUnqueue *u = (DelayUnqueue *)e;
  return cp_unparse_microseconds(u->_delay) + "\n";
}

void
DelayUnqueue::add_handlers()
{
  add_read_handler("delay", read_param, (void *)0);
  add_task_handlers(&_task);
}

EXPORT_ELEMENT(DelayUnqueue)
ELEMENT_MT_SAFE(DelayUnqueue)

