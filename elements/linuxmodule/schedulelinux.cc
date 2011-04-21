/*
 * schedulelinux.{cc,hh} -- go back to linux scheduler
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
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include "schedulelinux.hh"
#include <click/error.hh>
#include <click/glue.hh>

ScheduleLinux::ScheduleLinux()
  : _task(this)
{
}

ScheduleLinux::~ScheduleLinux()
{
}

int
ScheduleLinux::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

bool
ScheduleLinux::run_task(Task *)
{
  schedule();
  _task.fast_reschedule();
  return true;
}

void
ScheduleLinux::add_handlers()
{
  add_task_handlers(&_task);
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(ScheduleLinux)
