/*
 * ratedsplitter.{cc,hh} -- split packets at a given rate.
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
#include "ratedsplitter.hh"
#include "glue.hh"
#include "error.hh"
#include "confparse.hh"

int
RatedSplitter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned r;
  if (cp_va_parse(conf, this, errh, 
	          cpUnsigned, "split rate", &r, 0) < 0) 
    return -1;
  set_rate(r);
  return 0;
}

void
RatedSplitter::set_rate(int r)
{
  _meter = r;
  
  if (r > 0) _ugap = 1000000 / r;
  else _ugap = 1;

  _total = 0;
  _start.tv_sec = 0;
  _start.tv_usec = 0;
}

void
RatedSplitter::push(int, Packet *p)
{
  struct timeval now;
  click_gettimeofday(&now);

  if (_start.tv_sec == 0) _start = now;
  else {
    struct timeval diff;
    timersub(&now, &_start, &diff);
    
    unsigned need = diff.tv_sec * _meter;
    need += diff.tv_usec / _ugap;

    if (need > _total && _meter > 0) {
      _total++;
      output(1).push(p);
    } else
      output(0).push(p);

    if (_total > _meter * 360) {
      _total = 0;
      _start = now;
    }
  }
}


// HANDLERS

static int
rate_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  RatedSplitter* me = (RatedSplitter *) e;

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
  RatedSplitter *me = (RatedSplitter *) e;
  return String(me->get_rate()) + "\n";
}

void
RatedSplitter::add_handlers()
{
  add_read_handler("rate", rate_read_handler, 0);
  add_write_handler("rate", rate_write_handler, 0);
}

EXPORT_ELEMENT(RatedSplitter)


