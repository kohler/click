/*
 * discard.{cc,hh} -- element throws away all packets
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
#include "discard.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/standard/scheduleinfo.hh>
CLICK_DECLS

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
  if (input_is_pull(0)) {
    ScheduleInfo::initialize_task(this, &_task, errh);
    _signal = Notifier::upstream_pull_signal(this, 0, &_task);
  }
  return 0;
}

void
Discard::push(int, Packet *p)
{
  p->kill();
}

bool
Discard::run_task()
{
  Packet *p = input(0).pull();
  if (p)
    p->kill();
  else if (!_signal)
    return false;
  _task.fast_reschedule();
  return p != 0;
}

void
Discard::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Discard)
ELEMENT_MT_SAFE(Discard)
