/*
 * ratedsplitter.{cc,hh} -- split packets at a given rate.
 * Benjie Chen, Eddie Kohler
 * 
 * Copyright (c) 2000 Mazu Networks, Inc.
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
  set_rate(r, errh);
  return 0;
}

void
RatedSplitter::set_rate(unsigned r, ErrorHandler *errh)
{
  unsigned one_sec = 1000000 << UGAP_SHIFT;
  if (r > one_sec) {
    // must have _ugap > 0, so limit rate to 1e6<<UGAP_SHIFT
    if (errh)
      errh->error("rate too large; lowered to %u", one_sec);
    r = one_sec;
  }

  _rate = r;
  _ugap = one_sec / (r > 1 ? r : 1);
  _sec_count = 0;
  _tv_sec = -1;
}

void
RatedSplitter::push(int, Packet *p)
{
  struct timeval now;
  click_gettimeofday(&now);
  
  if (_tv_sec < 0) {
    _tv_sec = now.tv_sec;
    _sec_count = (now.tv_usec << UGAP_SHIFT) / _ugap;
  } else if (now.tv_sec > _tv_sec) {
    _tv_sec = now.tv_sec;
    if (_sec_count > 0)
      _sec_count -= _rate;
  }

  unsigned need = (now.tv_usec << UGAP_SHIFT) / _ugap;
  if ((int)need > _sec_count) {
#if DEBUG_RATEDSPLITTER
    static struct timeval last;
    if (last.tv_sec) {
      struct timeval diff;
      timersub(&now, &last, &diff);
      click_chatter("%d.%06d  (%d)", diff.tv_sec, diff.tv_usec, now.tv_sec);
    }
    last = now;
#endif
    output(0).push(p);
    _sec_count++;
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
  me->set_configuration_argument(0, conf);
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
