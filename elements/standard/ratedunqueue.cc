/*
 * ratedunqueue.{cc,hh} -- element pulls as many packets as possible from
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
#include "ratedunqueue.hh"
#include "confparse.hh"
#include "error.hh"
#include "elements/standard/scheduleinfo.hh"

int
RatedUnqueue::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned r;
  if (cp_va_parse(conf, this, errh, 
	          cpUnsigned, "unqueueing rate", &r, 0) < 0) 
    return -1;
  set_rate(r, errh);
  return 0;
}

int
RatedUnqueue::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
RatedUnqueue::uninitialize()
{
  unschedule();
}

void
RatedUnqueue::set_rate(unsigned r, ErrorHandler *errh)
{
  _rate.set_rate(r, errh);
}

void
RatedUnqueue::run_scheduled()
{
  struct timeval now;
  click_gettimeofday(&now);
  if (_rate.need_update(now)) {
    if (Packet *p = input(0).pull()) {
      _rate.update();
      output(0).push(p);
    }
  }
}


// HANDLERS

static int
rate_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  RatedUnqueue *me = (RatedUnqueue *)e;
  unsigned r;
  if (!cp_unsigned(cp_uncomment(conf), &r))
    return errh->error("rate must be an integer");
  me->set_rate(r);
  me->set_configuration_argument(0, conf);
  return 0;
}

static String
rate_read_handler(Element *e, void *)
{
  RatedUnqueue *me = (RatedUnqueue *) e;
  return String(me->rate()) + "\n";
}

void
RatedUnqueue::add_handlers()
{
  add_read_handler("rate", rate_read_handler, 0);
  add_write_handler("rate", rate_write_handler, 0);
}

EXPORT_ELEMENT(RatedUnqueue)
