/*
 * pulltopush.{cc,hh} -- element pulls as many packets as possible from
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
#include "pulltopush.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include "elements/standard/scheduleinfo.hh"

PullToPush::PullToPush()
  : Element(1, 1), _task(this)
{
  MOD_INC_USE_COUNT;
}

PullToPush::~PullToPush()
{
  MOD_DEC_USE_COUNT;
}

int
PullToPush::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  errh->error("PullToPush has been renamed; use Unqueue instead\n(PullToPush will be removed entirely in the next release.)");
  _burst = 1;
  return cp_va_parse(conf, this, errh,
		     cpOptional,
		     cpUnsigned, "burst size", &_burst,
		     0);
}

int
PullToPush::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
PullToPush::uninitialize()
{
  _task.unschedule();
}

void
PullToPush::run_scheduled()
{
  // XXX reduce # of tickets if idle
  for (int i = 0; i < _burst; i++)
    if (Packet *p = input(0).pull())
      output(0).push(p);
  _task.fast_reschedule();
}

void
PullToPush::add_handlers()
{
  add_task_handlers(&_task);
}

EXPORT_ELEMENT(PullToPush)
