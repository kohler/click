/*
 * discard.{cc,hh} -- element throws away all packets
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
#include "discard.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include "scheduleinfo.hh"

Discard::Discard()
  : Element(1, 0), _task(this)
{
  MOD_INC_USE_COUNT;
}

Discard::~Discard()
{
  MOD_DEC_USE_COUNT;
}

int
Discard::initialize(ErrorHandler *errh)
{
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
Discard::uninitialize()
{
  _task.unschedule();
}

void
Discard::push(int, Packet *p)
{
  p->kill();
}

void
Discard::run_scheduled()
{
  if (Packet *p = input(0).pull())
    p->kill();
  _task.fast_reschedule();
}

void
Discard::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

EXPORT_ELEMENT(Discard)
ELEMENT_MT_SAFE(Discard)
