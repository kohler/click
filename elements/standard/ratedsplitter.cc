/*
 * ratedsplitter.{cc,hh} -- split packets at a given rate.
 * Benjie Chen, Eddie Kohler
 * 
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "ratedsplitter.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>

RatedSplitter::RatedSplitter()
  : Element(1, 2)
{
  MOD_INC_USE_COUNT;
}

RatedSplitter::~RatedSplitter()
{
  MOD_DEC_USE_COUNT;
}

int
RatedSplitter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  unsigned r;
  if (cp_va_parse(conf, this, errh, 
	          cpUnsigned, "split rate", &r, 0) < 0) 
    return -1;
  set_rate(r, errh);
  return 0;
}

void
RatedSplitter::configuration(Vector<String> &conf, bool *) const
{
  conf.push_back(String(rate()));
}

void
RatedSplitter::set_rate(unsigned r, ErrorHandler *errh)
{
  _rate.set_rate(r, errh);
}

void
RatedSplitter::push(int, Packet *p)
{
  struct timeval now;
  click_gettimeofday(&now);
  if (_rate.need_update(now)) {
    _rate.update();
    output(0).push(p);
  } else
    output(1).push(p);
}


// HANDLERS

static int
rate_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  RatedSplitter *me = (RatedSplitter *)e;
  unsigned r;
  if (!cp_unsigned(cp_uncomment(conf), &r))
    return errh->error("rate must be an integer");
  me->set_rate(r);
  return 0;
}

static String
rate_read_handler(Element *e, void *)
{
  RatedSplitter *me = (RatedSplitter *) e;
  return String(me->rate()) + "\n";
}

void
RatedSplitter::add_handlers()
{
  add_read_handler("rate", rate_read_handler, 0);
  add_write_handler("rate", rate_write_handler, 0);
}

EXPORT_ELEMENT(RatedSplitter)
