/*
 * ratedsource.{cc,hh} -- generates configurable rated stream of packets.
 * Benjie Chen, Eddie Kohler (based on udpgen.o)
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2008 Regents of the University of California
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
#include "ratedsource.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/glue.hh>
CLICK_DECLS

const unsigned RatedSource::NO_LIMIT;

RatedSource::RatedSource()
  : _packet(0), _task(this), _timer(&_task)
{
}

int
RatedSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String data =
	"Random bullshit in a packet, at least 64 bytes long. Well, now it is.";
    unsigned rate = 10;
    unsigned bandwidth = 0;
    int limit = -1;
    int datasize = -1;
    bool active = true, stop = false;

    if (Args(conf, this, errh)
	.read_p("DATA", data)
	.read_p("RATE", rate)
	.read_p("LIMIT", limit)
	.read_p("ACTIVE", active)
	.read("LENGTH", datasize)
	.read("DATASIZE", datasize) // deprecated
	.read("STOP", stop)
	.read("BANDWIDTH", BandwidthArg(), bandwidth)
	.complete() < 0)
	return -1;

    _data = data;
    _datasize = datasize;
    if (bandwidth > 0)
	rate = bandwidth / (_datasize < 0 ? _data.length() : _datasize);
    int burst = rate < 200 ? 2 : rate / 100;
    if (bandwidth > 0 && burst < 2 * datasize)
	burst = 2 * datasize;
    _tb.assign(rate, burst);
    _limit = (limit >= 0 ? unsigned(limit) : NO_LIMIT);
    _active = active;
    _stop = stop;

    setup_packet();

    return 0;
}

int
RatedSource::initialize(ErrorHandler *errh)
{
    _count = 0;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, errh);
    _tb.set(1);
    _timer.initialize(this);
    return 0;
}

void
RatedSource::cleanup(CleanupStage)
{
    if (_packet)
	_packet->kill();
    _packet = 0;
}

bool
RatedSource::run_task(Task *)
{
    if (!_active)
	return false;
    if (_limit != NO_LIMIT && _count >= _limit) {
	if (_stop)
	    router()->please_stop_driver();
	return false;
    }

    _tb.refill();
    if (_tb.remove_if(1)) {
	Packet *p = _packet->clone();
	p->set_timestamp_anno(Timestamp::now());
	output(0).push(p);
	_count++;
	_task.fast_reschedule();
	return true;
    } else {
	_timer.schedule_after(Timestamp::make_jiffies(_tb.time_until_contains(1)));
	return false;
    }
}

Packet *
RatedSource::pull(int)
{
    if (!_active)
	return 0;
    if (_limit != NO_LIMIT && _count >= _limit) {
	if (_stop)
	    router()->please_stop_driver();
	return 0;
    }

    _tb.refill();
    if (_tb.remove_if(1)) {
	_count++;
	Packet *p = _packet->clone();
	p->set_timestamp_anno(Timestamp::now());
	return p;
    } else
	return 0;
}

void
RatedSource::setup_packet()
{
    if (_packet)
	_packet->kill();

    // note: if you change `headroom', change `click-align'
    unsigned int headroom = 16+20+24;

    if (_datasize < 0)
	_packet = Packet::make(headroom, (unsigned char *) _data.data(), _data.length(), 0);
    else if (_datasize <= _data.length())
	_packet = Packet::make(headroom, (unsigned char *) _data.data(), _datasize, 0);
    else {
	// make up some data to fill extra space
	StringAccum sa;
	while (sa.length() < _datasize)
	    sa << _data;
	_packet = Packet::make(headroom, (unsigned char *) sa.data(), _datasize, 0);
    }
}

String
RatedSource::read_param(Element *e, void *vparam)
{
  RatedSource *rs = (RatedSource *)e;
  switch ((intptr_t)vparam) {
   case 0:			// data
    return rs->_data;
   case 1:			// rate
    return String(rs->_tb.rate());
   case 2:			// limit
    return (rs->_limit != NO_LIMIT ? String(rs->_limit) : String("-1"));
   default:
    return "";
  }
}

int
RatedSource::change_param(const String &s, Element *e, void *vparam,
			  ErrorHandler *errh)
{
  RatedSource *rs = (RatedSource *)e;
  switch ((intptr_t)vparam) {

  case 0:			// data
      rs->_data = s;
      if (rs->_packet)
	  rs->_packet->kill();
      rs->_packet = Packet::make(rs->_data.data(), rs->_data.length());
      break;

  case 1: {			// rate
      unsigned rate;
      if (!IntArg().parse(s, rate))
	  return errh->error("syntax error");
      rs->_tb.assign_adjust(rate, rate < 200 ? 2 : rate / 100);
      break;
  }

   case 2: {			// limit
     int limit;
     if (!IntArg().parse(s, limit))
       return errh->error("syntax error");
     rs->_limit = (limit >= 0 ? unsigned(limit) : NO_LIMIT);
     break;
   }

  case 3: {			// active
      bool active;
      if (!BoolArg().parse(s, active))
	  return errh->error("syntax error");
      rs->_active = active;
      if (rs->output_is_push(0) && !rs->_task.scheduled() && active) {
	  rs->_tb.set(1);
	  rs->_task.reschedule();
      }
      break;
  }

  case 5: {			// reset
      rs->_count = 0;
      rs->_tb.set(1);
      if (rs->output_is_push(0) && !rs->_task.scheduled() && rs->_active)
	  rs->_task.reschedule();
      break;
  }

  case 6: {			// datasize
      int datasize;
      if (!IntArg().parse(s, datasize))
	  return errh->error("syntax error");
      rs->_datasize = datasize;
      rs->setup_packet();
      break;
  }
  }
  return 0;
}

void
RatedSource::add_handlers()
{
  add_read_handler("data", read_param, 0, Handler::CALM);
  add_write_handler("data", change_param, 0, Handler::RAW);
  add_read_handler("rate", read_param, 1);
  add_write_handler("rate", change_param, 1);
  add_read_handler("limit", read_param, 2, Handler::CALM);
  add_write_handler("limit", change_param, 2);
  add_data_handlers("active", Handler::OP_READ | Handler::CHECKBOX, &_active);
  add_write_handler("active", change_param, 3);
  add_data_handlers("count", Handler::OP_READ, &_count);
  add_write_handler("reset", change_param, 5, Handler::BUTTON);
  add_data_handlers("length", Handler::OP_READ, &_datasize);
  add_write_handler("length", change_param, 6);
  // deprecated
  add_data_handlers("datasize", Handler::OP_READ | Handler::DEPRECATED, &_datasize);
  add_write_handler("datasize", change_param, 6);

  if (output_is_push(0))
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RatedSource)
