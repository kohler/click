/*
 * timedsink.{cc,hh} -- element creates packets, pushes them periodically
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
#include "timedsource.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

TimedSource::TimedSource()
  : _data("Random bullshit in a packet, at least 64 byte long.  Well, now it is."),
    _timer(this)
{
  add_output();
}

TimedSource *
TimedSource::clone() const
{
  return new TimedSource;
}

int
TimedSource::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpInterval, "packet generation interval", &_interval,
		     cpOptional,
		     cpString, "packet data", &_data,
		     0);
}

int
TimedSource::initialize(ErrorHandler *)
{
  _timer.attach(this);
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
