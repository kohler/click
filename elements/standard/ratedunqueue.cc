/*
 * ratedunqueue.{cc,hh} -- element pulls as many packets as possible from
 * its input, pushes them out its output
 * Eddie Kohler
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
#include <click/package.hh>
#include "ratedunqueue.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include "elements/standard/scheduleinfo.hh"

RatedUnqueue::RatedUnqueue()
  : Element(1, 1), _task(this)
{
  MOD_INC_USE_COUNT;
}

RatedUnqueue::~RatedUnqueue()
{
  MOD_DEC_USE_COUNT;
}

int
RatedUnqueue::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned r;
  if (cp_va_parse(conf, this, errh, 
	          cpUnsigned, "unqueueing rate", &r, 0) < 0) 
    return -1;
  set_rate(r, errh);
  return 0;
}

void
RatedUnqueue::configuration(Vector<String> &conf) const
{
  conf.push_back(String(rate()));
}

int
RatedUnqueue::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
RatedUnqueue::uninitialize()
{
  _task.unschedule();
}

void
RatedUnqueue::set_rate(unsigned r, ErrorHandler *errh)
{
  _rate.set_rate(r, errh);
}

void
RatedUnqueue::run_scheduled()
{
  struct timeval now;
  click_gettimeofday(&now);
  if (_rate.need_update(now)) {
    if (Packet *p = input(0).pull()) {
      _rate.update();
      output(0).push(p);
    }
  }
  _task.fast_reschedule();
}


// HANDLERS

static int
rate_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  RatedUnqueue *me = (RatedUnqueue *)e;
  unsigned r;
  if (!cp_unsigned(cp_uncomment(conf), &r))
    return errh->error("rate must be an integer");
  me->set_rate(r);
  return 0;
}

static String
rate_read_handler(Element *e, void *)
{
  RatedUnqueue *me = (RatedUnqueue *) e;
  return String(me->rate()) + "\n";
}

void
RatedUnqueue::add_handlers()
{
  add_read_handler("rate", rate_read_handler, 0);
  add_write_handler("rate", rate_write_handler, 0);
  add_task_handlers(&_task);
}

EXPORT_ELEMENT(RatedUnqueue)
ELEMENT_MT_SAFE(RatedUnqueue)
