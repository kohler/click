/*
 * timedsink.{cc,hh} -- element pulls packets periodically, discards them
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
#include "timedsink.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

TimedSink::TimedSink()
  : Element(1, 0), _timer(this)
{
  MOD_INC_USE_COUNT;
}

TimedSink::~TimedSink()
{
  MOD_DEC_USE_COUNT;
}

TimedSink *
TimedSink::clone() const
{
  return new TimedSink;
}

int
TimedSink::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpMilliseconds, "packet pull interval", &_interval,
		     0);
}

int
TimedSink::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(_interval);
  return 0;
}

void
TimedSink::uninitialize()
{
  _timer.unschedule();
}

void
TimedSink::run_scheduled()
{
  Packet *p = input(0).pull();
  if (p)
    p->kill();
  _timer.reschedule_after_ms(_interval);
}

EXPORT_ELEMENT(TimedSink)
ELEMENT_MT_SAFE(TimedSink)
