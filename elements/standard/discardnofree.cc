/*
 * discardnofree.{cc,hh} -- element pulls packets, doesn't throw them away
 * Robert Morris
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
#include "discardnofree.hh"
#include "scheduleinfo.hh"

DiscardNoFree::DiscardNoFree()
  : Element(1, 0), _task(this)
{
  MOD_INC_USE_COUNT;
}

DiscardNoFree::~DiscardNoFree()
{
  MOD_DEC_USE_COUNT;
}

int
DiscardNoFree::initialize(ErrorHandler *errh)
{
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
DiscardNoFree::uninitialize()
{
  _task.unschedule();
}

void
DiscardNoFree::push(int, Packet *)
{
  // Don't kill().
}

void
DiscardNoFree::run_scheduled()
{
  (void) input(0).pull();
  _task.fast_reschedule();
}

void
DiscardNoFree::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

EXPORT_ELEMENT(DiscardNoFree)
ELEMENT_MT_SAFE(DiscardNoFree)
