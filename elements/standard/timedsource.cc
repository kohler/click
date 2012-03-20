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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/router.hh>
CLICK_DECLS

TimedSource::TimedSource()
    : _packet(0), _interval(0, Timestamp::subsec_per_sec / 2), _limit(-1),
      _count(0), _active(true), _stop(false), _timer(this),
      _headroom(Packet::default_headroom)
{
}

int
TimedSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String data = "Random bullshit in a packet, at least 64 bytes long. Well, now it is.";

    if (Args(conf, this, errh)
	.read_p("INTERVAL", _interval)
	.read_p("DATA", data)
	.read("LIMIT", _limit)
	.read("ACTIVE", _active)
	.read("STOP", _stop)
	.read("HEADROOM", _headroom)
	.complete() < 0)
	return -1;

    _data = data;
    if (_packet)
	_packet->kill();
    _packet = Packet::make(_headroom, _data.data(), _data.length(), 0);
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
	p->timestamp_anno().assign_now();
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
    case h_interval:
	return ts->_interval.unparse_interval();
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

    case h_data:
	ts->_data = s;
	goto remake_packet;

    case h_headroom:
	if (!IntArg().parse(s, ts->_headroom))
	    return errh->error("bad headroom");
	goto remake_packet;

    remake_packet: {
	Packet *p = Packet::make(ts->_headroom, ts->_data.data(), ts->_data.length(), 0);
	if (!p)
	    return errh->error("out of memory"), -ENOMEM;
	if (ts->_packet)
	    ts->_packet->kill();
	ts->_packet = p;
	break;
    }

   case h_interval: {
     Timestamp interval;
     if (!cp_time(s, &interval) || !interval)
       return errh->error("bad interval");
     ts->_interval = interval;
     break;
   }

   case h_active: {
       if (!BoolArg().parse(s, ts->_active))
       return errh->error("bad active");
     if (!ts->_timer.scheduled() && ts->_active)
       ts->_timer.schedule_now();
     break;
   }

   case h_reset: {
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
    add_data_handlers("data", Handler::OP_READ | Handler::CALM, &_data);
    add_write_handler("data", change_param, h_data, Handler::RAW);
    add_data_handlers("limit", Handler::OP_READ | Handler::OP_WRITE | Handler::CALM, &_limit);
    add_read_handler("interval", read_param, h_interval, Handler::CALM);
    add_write_handler("interval", change_param, h_interval);
    add_data_handlers("active", Handler::OP_READ | Handler::CALM | Handler::CHECKBOX, &_active);
    add_write_handler("active", change_param, h_active);
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_data_handlers("headroom", Handler::OP_READ | Handler::CALM, &_headroom);
    add_write_handler("headroom", change_param, h_headroom);
    add_write_handler("reset", change_param, h_reset, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimedSource)
ELEMENT_MT_SAFE(TimedSource)
