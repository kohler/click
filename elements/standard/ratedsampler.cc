/*
 * ratedsampler.{cc,hh} -- samples packets with certain probability.
 *
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
#include "ratedsampler.hh"
#include "glue.hh"
#include "error.hh"
#include "confparse.hh"

int
RatedSampler::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned r;
  if (cp_va_parse(conf, this, errh, 
	          cpUnsigned, "sample rate", &r, 0) < 0) 
    return -1;
  set_rate(r);
  return 0;
}

void
RatedSampler::set_rate(int r)
{
  _meter = r;
  _ugap = 1000000 / r;
  _total = 0;
  _start.tv_sec = 0;
  _start.tv_usec = 0;
}

void
RatedSampler::push(int, Packet *p)
{
  struct timeval now;
  click_gettimeofday(&now);

  if (_start.tv_sec == 0) _start = now;
  else {
    struct timeval diff;
    timersub(&now, &_start, &diff);
    
    unsigned need = diff.tv_sec * _meter;
    need += diff.tv_usec / _ugap;

    if (need > _total) {
      output(1).push(p->clone());
      _total++;
    }

    if (_total > _meter * 360) {
      _total = 0;
      _start = now;
    }
  }
  output(0).push(p);
}


// HANDLERS

static int
rate_write_handler(const String &conf, Element *e, 
                                 void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  RatedSampler* me = (RatedSampler *) e;

  if(args.size() != 1)
    return errh->error("expecting one number");

  int r;
  if (!cp_integer(args[0], &r))
    return errh->error("rate must be an integer");
  
  me->set_rate(r);
  return 0;
}

static String
rate_read_handler(Element *e, void *)
{
  RatedSampler *me = (RatedSampler *) e;
  return String(me->get_rate()) + "\n";
}

void
RatedSampler::add_handlers()
{
  add_read_handler("rate", rate_read_handler, 0);
  add_write_handler("rate", rate_write_handler, 0);
}

EXPORT_ELEMENT(RatedSampler)


