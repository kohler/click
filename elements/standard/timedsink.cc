/*
 * timedsink.{cc,hh} -- element pulls packets periodically, discards them
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "timedsink.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

TimedSink::TimedSink()
  : _timer(this)
{
  add_input();
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
  _timer.attach(this);
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
  _timer.schedule_after_ms(_interval);
}

EXPORT_ELEMENT(TimedSink)
