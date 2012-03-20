/*
 * discardnofree.{cc,hh} -- element pulls packets, doesn't throw them away
 * Robert Morris
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
#include "discardnofree.hh"
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

DiscardNoFree::DiscardNoFree()
  : _task(this)
{
}

int
DiscardNoFree::initialize(ErrorHandler *errh)
{
  if (input_is_pull(0))
    ScheduleInfo::initialize_task(this, &_task, errh);
  return 0;
}

void
DiscardNoFree::push(int, Packet *)
{
  // Don't kill().
}

bool
DiscardNoFree::run_task(Task *)
{
  Packet *p = input(0).pull();	// Not killed!
  _task.fast_reschedule();
  return p != 0;
}

void
DiscardNoFree::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DiscardNoFree)
ELEMENT_MT_SAFE(DiscardNoFree)
