/*
 * timedsink.{cc,hh} -- element creates packets, pushes them periodically
 * Eddie Kohler
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
#include "timedsource.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/router.hh>
CLICK_DECLS

TimedSource::TimedSource()
    : _packet(0), _interval(0, Timestamp::subsec_per_sec / 2), _timer(this)
{
}

TimedSource::~TimedSource()
{
}

int
TimedSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String data = "Random bullshit in a packet, at least 64 bytes long. Well, now it is.";
  int limit = -1;
  bool active = true, stop = false;

  if (cp_va_kparse(conf, this, errh,
		   "INTERVAL", cpkP, cpTimestamp, &_interval,
		   "DATA", cpkP, cpString, &data,
		   "LIMIT", 0, cpInteger, &limit,
		   "ACTIVE", 0, cpBool, &active,
		   "STOP", 0, cpBool, &stop,
		   cpEnd) < 0)
    return -1;

  _data = data;
  _limit = limit;
  _count = 0;
  _active = active;
  _stop = stop;
  if (_packet) _packet->kill();
  _packet = Packet::make(_data.data(), _data.length());
  return 0;
}

int
TimedSource::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  if (_active)
    _timer.schedule_after(_interval);
  return 0;
}

void
TimedSource::cleanup(CleanupStage)
{
  if (_packet)
    _packet->kill();
  _packet = 0;
}

void
TimedSource::run_timer(Timer *)
{
    if (!_active)
	return;
    if (_limit < 0 || _count < _limit) {
	Packet *p = _packet->clone();
	p->timestamp_anno().set_now();
	output(0).push(p);
	_count++;
	_timer.reschedule_after(_interval);
    } else if (_stop)
	router()->please_stop_driver();
}

String
TimedSource::read_param(Element *e, void *vparam)
{
  TimedSource *ts = (TimedSource *)e;
  switch ((intptr_t)vparam) {
   case 0:			// data
    return ts->_data;
   case 2:			// interval
    return ts->_interval.unparse_interval();
   case 3:			// active
    return cp_unparse_bool(ts->_active);
   default:
    return "";
  }
}

int
TimedSource::change_param(const String &s, Element *e, void *vparam,
			  ErrorHandler *errh)
{
  TimedSource *ts = (TimedSource *)e;
  switch ((intptr_t)vparam) {

  case 0:			// data
      ts->_data = s;
      if (ts->_packet)
	  ts->_packet->kill();
      ts->_packet = Packet::make(ts->_data.data(), ts->_data.length());
      break;
   
   case 2: {			// interval
     Timestamp interval;
     if (!cp_time(s, &interval) || !interval)
       return errh->error("bad interval");
     ts->_interval = interval;
     break;
   }
   
   case 3: {			// active
     bool active;
     if (!cp_bool(s, &active))
       return errh->error("bad active");
     ts->_active = active;
     if (!ts->_timer.scheduled() && active)
       ts->_timer.schedule_now();
     break;
   }

   case 5: {			// reset
     ts->_count = 0;
     if (!ts->_timer.scheduled() && ts->_active)
       ts->_timer.schedule_now();
     break;
   }

  }
  return 0;
}

void
TimedSource::add_handlers()
{
  add_read_handler("data", read_param, (void *)0, Handler::CALM);
  add_write_handler("data", change_param, (void *)0, Handler::RAW);
  add_data_handlers("limit", Handler::OP_READ | Handler::OP_WRITE | Handler::CALM, &_limit);
  add_read_handler("interval", read_param, (void *)2, Handler::CALM);
  add_write_handler("interval", change_param, (void *)2);
  add_read_handler("active", read_param, (void *)3, Handler::CALM | Handler::CHECKBOX);
  add_write_handler("active", change_param, (void *)3);
  add_data_handlers("count", Handler::OP_READ, &_count);
  add_write_handler("reset", change_param, (void *)5, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimedSource)
ELEMENT_MT_SAFE(TimedSource)
