/*
 * counter2.{cc,hh} -- element counts packets, measures duration 
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
#include "counter2.hh"
#include "confparse.hh"
#include "glue.hh"
#include "error.hh"

static String counter2_read_rate_handler(Element *, void *);

Counter2::Counter2()
  : Element(1, 1)
{
}

void
Counter2::reset()
{
  _count = 0;
  _first = 0;
  _last = 0;
}

int
Counter2::initialize(ErrorHandler *)
{
  reset();
  return 0;
}

Packet *
Counter2::simple_action(Packet *p)
{
  _count++;
  if (_first == 0) _first = click_jiffies();
  _last = click_jiffies();
  return p;
}

static String
counter2_read_count_handler(Element *e, void *)
{
  Counter2 *c = (Counter2 *)e;
  return String(c->count()) + "\n";
}

static String
counter2_read_rate_handler(Element *e, void *)
{
  Counter2 *c = (Counter2 *)e;
  int d = c->last() - c->first();
  if (d < 1) d = 1;
  int rate = c->count() * CLICK_HZ / d;
  return String(rate) + "\n";
}

static int
counter2_reset_write_handler(const String &, Element *e, void *, ErrorHandler *)
{
  Counter2 *c = (Counter2 *)e;
  c->reset();
  return 0;
}

void
Counter2::add_handlers()
{
  add_read_handler("count", counter2_read_count_handler, 0);
  add_read_handler("rate", counter2_read_rate_handler, 0);
  add_write_handler("reset", counter2_reset_write_handler, 0);
}


EXPORT_ELEMENT(Counter2)
