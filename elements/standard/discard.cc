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
    : _task(this), _count(0)
{
}

Discard::~Discard()
{
}

int
Discard::initialize(ErrorHandler *errh)
{
    if (input_is_pull(0)) {
	ScheduleInfo::initialize_task(this, &_task, errh);
	_signal = Notifier::upstream_empty_signal(this, 0, &_task);
    }
    return 0;
}

void
Discard::push(int, Packet *p)
{
    _count++;
    p->kill();
}

bool
Discard::run_task(Task *)
{
  Packet *p = input(0).pull();
  if (p) {
      _count++;
      p->kill();
  } else if (!_signal)
    return false;
  _task.fast_reschedule();
  return p != 0;
}

String
Discard::read_handler(Element *e, void *)
{
    Discard *d = static_cast<Discard *>(e);
    return String(d->_count);
}

int
Discard::write_handler(const String &, Element *e, void *, ErrorHandler *)
{
    Discard *d = static_cast<Discard *>(e);
    d->_count = 0;
    return 0;
}

void
Discard::add_handlers()
{
    add_read_handler("count", read_handler, 0);
    add_write_handler("reset_counts", write_handler, 0);
    if (input_is_pull(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Discard)
ELEMENT_MT_SAFE(Discard)
