/*
 * averagecounter.{cc,hh} -- element counts packets, measures duration 
 * Benjie Chen
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
#include "averagecounter.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "glue.hh"
#include "error.hh"

AverageCounter::AverageCounter()
  : Element(1, 1)
{
}

void
AverageCounter::reset()
{
  _count = 0;
  _first = 0;
  _last = 0;
}

int
AverageCounter::initialize(ErrorHandler *)
{
  reset();
  return 0;
}

Packet *
AverageCounter::simple_action(Packet *p)
{
  _count++;
  if (_first == 0) _first = click_jiffies();
  _last = click_jiffies();
  return p;
}

static String
averagecounter_read_count_handler(Element *e, void *)
{
  AverageCounter *c = (AverageCounter *)e;
  return String(c->count()) + "\n";
}

static String
averagecounter_read_rate_handler(Element *e, void *)
{
  AverageCounter *c = (AverageCounter *)e;
  int d = c->last() - c->first();
  if (d < 1) d = 1;
  int rate = c->count() * CLICK_HZ / d;
  return String(rate) + "\n";
}

static int
averagecounter_reset_write_handler
(const String &, Element *e, void *, ErrorHandler *)
{
  AverageCounter *c = (AverageCounter *)e;
  c->reset();
  return 0;
}

void
AverageCounter::add_handlers()
{
  add_read_handler("count", averagecounter_read_count_handler, 0);
  add_read_handler("average", averagecounter_read_rate_handler, 0);
  add_write_handler("reset", averagecounter_reset_write_handler, 0);
}


EXPORT_ELEMENT(AverageCounter)
