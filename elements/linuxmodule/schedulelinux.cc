/*
 * schedulelinux.{cc,hh} -- go back to linux scheduler
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
#include <click/router.hh>
#include "elements/standard/scheduleinfo.hh"
#include "schedulelinux.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <unistd.h>

ScheduleLinux::ScheduleLinux()
  : _task(this)
{
  MOD_INC_USE_COUNT;
}

ScheduleLinux::~ScheduleLinux()
{
  MOD_DEC_USE_COUNT;
}

ScheduleLinux *
ScheduleLinux::clone() const
{
  return new ScheduleLinux();
}

int
ScheduleLinux::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh, cpEnd);
}

int
ScheduleLinux::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
ScheduleLinux::uninitialize()
{
  _task.unschedule();
}

void
ScheduleLinux::run_scheduled()
{
  schedule();
  _task.fast_reschedule();
}

void
ScheduleLinux::add_handlers()
{
  add_task_handlers(&_task);
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(ScheduleLinux)
