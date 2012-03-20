/*
 * averagecounter.{cc,hh} -- element counts packets, measures duration
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "averagecounter.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/sync.hh>
#include <click/glue.hh>
#include <click/error.hh>
CLICK_DECLS

AverageCounter::AverageCounter()
{
}

void
AverageCounter::reset()
{
  _count = 0;
  _byte_count = 0;
  _first = 0;
  _last = 0;
}

int
AverageCounter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _ignore = 0;
  if (Args(conf, this, errh).read_p("IGNORE", _ignore).complete() < 0)
    return -1;
  _ignore *= CLICK_HZ;
  return 0;
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
    uint32_t jpart = click_jiffies();
    _first.compare_swap(0, jpart);
    if (jpart - _first >= _ignore) {
	_count++;
	_byte_count += p->length();
    }
    _last = jpart;
    return p;
}

static String
averagecounter_read_count_handler(Element *e, void *thunk)
{
  AverageCounter *c = (AverageCounter *)e;
  return String(thunk ? c->byte_count() : c->count());
}

static String
averagecounter_read_rate_handler(Element *e, void *thunk)
{
  AverageCounter *c = (AverageCounter *)e;
  uint32_t d = c->last() - c->first();
  d -= c->ignore();
  if (d < 1) d = 1;
  uint32_t count = (thunk ? c->byte_count() : c->count());
#if CLICK_USERLEVEL
  return String(((double) count * CLICK_HZ) / d);
#else
  uint32_t rate;
  if (count < (uint32_t) (0xFFFFFFFFU / CLICK_HZ))
      rate = (count * CLICK_HZ) / d;
  else
      rate = (count / d) * CLICK_HZ;
  return String(rate);
#endif
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
  add_read_handler("byte_count", averagecounter_read_count_handler, 1);
  add_read_handler("rate", averagecounter_read_rate_handler, 0);
  add_read_handler("byte_rate", averagecounter_read_rate_handler, 1);
  add_write_handler("reset", averagecounter_reset_write_handler, 0, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AverageCounter)
ELEMENT_MT_SAFE(AverageCounter)
