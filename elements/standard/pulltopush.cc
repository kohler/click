/*
 * pulltopush.{cc,hh} -- element pulls as many packets as possible from
 * its input, pushes them out its output
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
#include "pulltopush.hh"
#include "confparse.hh"
#include "elements/standard/scheduleinfo.hh"

int
PullToPush::configure(const String &conf, ErrorHandler *errh)
{
  _burst = 1;
  return cp_va_parse(conf, this, errh,
		     cpOptional,
		     cpUnsigned, "burst size", &_burst,
		     0);
}

int
PullToPush::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
PullToPush::uninitialize()
{
  unschedule();
}

void
PullToPush::run_scheduled()
{
  // XXX reduce # of tickets if idle
  for (int i = 0; i < _burst; i++)
    if (Packet *p = input(0).pull())
      output(0).push(p);
  reschedule();
}

EXPORT_ELEMENT(PullToPush)
