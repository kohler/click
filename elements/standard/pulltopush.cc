/*
 * pulltopush.{cc,hh} -- element pulls as many packets as possible from
 * its input, pushes them out its output
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
#include "pulltopush.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include "elements/standard/scheduleinfo.hh"

int
PullToPush::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  errh->error("PullToPush has been renamed; use Unqueue instead\n(PullToPush will be removed entirely in the next release.)");
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
