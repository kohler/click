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
#include "error.hh"

static String counter2_read_duration_handler(Element *, void *);


static inline unsigned long
sub_timer(struct timeval *tv2, struct timeval *tv1)
{
  struct timeval diff;
  diff.tv_sec = tv2->tv_sec - tv1->tv_sec;
  diff.tv_usec = tv2->tv_usec - tv1->tv_usec;
  if (diff.tv_usec < 0) {
    diff.tv_sec--;
    diff.tv_usec += 1000000;
  }
  return diff.tv_sec*1000000+diff.tv_usec;
}

Counter2::Counter2()
  : Element(1, 1), _count(0), _duration(0)
{
}

void
Counter2::reset()
{
  _count = 0;
  _duration = 0;
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
  struct timeval _now;
  _count++;
  click_gettimeofday(&_now);
  if (_count != 1)
    _duration += sub_timer(&_now, &_last);
  _last = _now;
  return p;
}

static String
counter2_read_count_handler(Element *e, void *)
{
  Counter2 *c = (Counter2 *)e;
  return String(c->count()) + "\n";
}

static String
counter2_read_duration_handler(Element *e, void *)
{
  Counter2 *c = (Counter2 *)e;
  return String(c->duration()) + "\n";
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
  add_read_handler("duration", counter2_read_duration_handler, 0);
  add_write_handler("reset", counter2_reset_write_handler, 0);
}


EXPORT_ELEMENT(Counter2)
