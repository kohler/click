/*
 * timedsink.{cc,hh} -- element pulls packets periodically, discards them
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
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
TimedSink::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpInterval, "packet pull interval", &_interval,
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
