/*
 * averagecounter.{cc,hh} -- element counts packets, measures duration 
 * Benjie Chen
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
#include <click/config.h>
#include <click/package.hh>
#include "averagecounter.hh"
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/glue.hh>
#include <click/error.hh>

AverageCounter::AverageCounter()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

AverageCounter::~AverageCounter()
{
  MOD_DEC_USE_COUNT;
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
  add_read_handler("rate", averagecounter_read_rate_handler, 0);
  add_write_handler("reset", averagecounter_reset_write_handler, 0);
}


EXPORT_ELEMENT(AverageCounter)
