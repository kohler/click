/*
 * timedsink.{cc,hh} -- element creates packets, pushes them periodically
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
#include "timedsource.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

TimedSource::TimedSource()
  : Element(0, 1),
    _data("Random bullshit in a packet, at least 64 bytes long. Well, now it is."),
    _timer(this)
{
  MOD_INC_USE_COUNT;
}

TimedSource::~TimedSource()
{
  MOD_DEC_USE_COUNT;
}

TimedSource *
TimedSource::clone() const
{
  return new TimedSource;
}

int
TimedSource::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpMilliseconds, "packet generation interval", &_interval,
		     cpOptional,
		     cpString, "packet data", &_data,
		     0);
}

int
TimedSource::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(_interval);
  return 0;
}

void
TimedSource::uninitialize()
{
  _timer.unschedule();
}

void
TimedSource::run_scheduled()
{
  output(0).push(Packet::make(_data.data(), _data.length()));
  _timer.schedule_after_ms(_interval);
}

EXPORT_ELEMENT(TimedSource)
ELEMENT_MT_SAFE(TimedSource)
