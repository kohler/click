/*
 * ratedunqueue.{cc,hh} -- element pulls as many packets as possible from
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
#include "ratedunqueue.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

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
RatedUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
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
  ScheduleInfo::initialize_task(this, &_task, errh);
  return 0;
}

void
RatedUnqueue::set_rate(unsigned r, ErrorHandler *errh)
{
  _rate.set_rate(r, errh);
}

bool
RatedUnqueue::run_task()
{
  struct timeval now;
  click_gettimeofday(&now);
  bool worked = false;
  if (_rate.need_update(now)) {
    if (Packet *p = input(0).pull()) {
      _rate.update();
      output(0).push(p);
      worked = true;
    }
  }
  _task.fast_reschedule();
  return worked;
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

CLICK_ENDDECLS
EXPORT_ELEMENT(RatedUnqueue)
ELEMENT_MT_SAFE(RatedUnqueue)
