// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * timefilter.{cc,hh} -- element filters packets by timestamp
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include <click/error.hh>
#include "timefilter.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/handlercall.hh>
CLICK_DECLS

TimeFilter::TimeFilter()
    : Element(1, 1), _last_h(0)
{
    MOD_INC_USE_COUNT;
}

TimeFilter::~TimeFilter()
{
    MOD_DEC_USE_COUNT;
    delete _last_h;
}

void
TimeFilter::notify_noutputs(int n)
{
    set_noutputs(n <= 1 ? 1 : 2);
}

int
TimeFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    struct timeval first, last, first_init, last_init, first_delta, last_delta, interval;
    timerclear(&first);
    timerclear(&last);
    timerclear(&first_init);
    timerclear(&last_init);
    timerclear(&first_delta);
    timerclear(&last_delta);
    timerclear(&interval);
    bool stop = false;

    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "START", cpTimeval, "start time", &first,
		    "END", cpTimeval, "end time", &last,
		    "START_DELAY", cpTimeval, "start T after initialization", &first_init,
		    "END_DELAY", cpTimeval, "end T after initialization", &last_init,
		    "START_AFTER", cpTimeval, "start T after first packet", &first_delta,
		    "END_AFTER", cpTimeval, "end T after first packet", &last_delta,
		    "INTERVAL", cpTimeval, "interval", &interval,
		    "STOP", cpBool, "stop when after end?", &stop,
		    "END_CALL", cpWriteHandlerCall, "handler to call at end", &_last_h,
		    0) < 0)
	return -1;

    _first_relative = _first_init_relative = _last_relative = _last_init_relative = _last_interval = false;
    
    if ((timerisset(&first) != 0) + (timerisset(&first_delta) != 0) + (timerisset(&first_init) != 0) > 1)
	return errh->error("`START', `START_AFTER', and `START_AFTER_INIT' are mutually exclusive");
    else if (timerisset(&first))
	_first = first;
    else if (timerisset(&first_init))
	_first = first_init, _first_init_relative = true;
    else
	_first = first_delta, _first_relative = true;
    
    if ((timerisset(&last) != 0) + (timerisset(&last_delta) != 0) + (timerisset(&last_init) != 0) + (timerisset(&interval) != 0) > 1)
	return errh->error("`END', `END_AFTER', `END_AFTER_INIT', and `INTERVAL'\nare mutually exclusive");
    else if (timerisset(&last))
	_last = last;
    else if (timerisset(&last_delta))
	_last = last_delta, _last_relative = true;
    else if (timerisset(&last_init))
	_last = last_init, _last_init_relative = true;
    else if (timerisset(&interval))
	_last = interval, _last_interval = true;
    else
	_last.tv_sec = 0x7FFFFFFF;

    if (_last_h && stop)
	return errh->error("`END_CALL' and `STOP' are mutually exclusive");
    else if (stop)
	_last_h = new HandlerCall("stop true");
    
    _ready = false;
    return 0;
}

int
TimeFilter::initialize(ErrorHandler *errh)
{
    if (_last_h && _last_h->initialize_write(this, errh) < 0)
	return -1;
    if (_first_init_relative || _last_init_relative) {
	struct timeval now;
	click_gettimeofday(&now);
	if (_first_init_relative)
	    timeradd(&_first, &_first, &now);
	if (_last_init_relative)
	    timeradd(&_last, &_last, &now);
    }
    _last_h_ready = (_last_h != 0);
    return 0;
}

void
TimeFilter::first_packet(const struct timeval &tv)
{
    if (_first_relative)
	timeradd(&tv, &_first, &_first);
    if (_last_relative)
	timeradd(&tv, &_last, &_last);
    else if (_last_interval)
	timeradd(&_first, &_last, &_last);
    _ready = true;
}

Packet *
TimeFilter::kill(Packet *p)
{
    checked_output_push(1, p);
    return 0;
}

Packet *
TimeFilter::simple_action(Packet *p)
{
    const struct timeval &tv = p->timestamp_anno();
    if (!_ready)
	first_packet(tv);
    if (timercmp(&tv, &_first, <))
	return kill(p);
    else if (timercmp(&tv, &_last, <))
	return p;
    else {
	if (_last_h && _last_h_ready) {
	    _last_h_ready = false;
	    (void) _last_h->call_write(this);
	}
	return kill(p);
    }
}


enum { H_EXTEND_INTERVAL };

int
TimeFilter::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    TimeFilter *tf = static_cast<TimeFilter *>(e);
    String s = cp_uncomment(s_in);
    switch ((intptr_t)thunk) {
      case H_EXTEND_INTERVAL: {
	  struct timeval tv;
	  if (cp_timeval(s, &tv)) {
	      timeradd(&tf->_last, &tv, &tf->_last);
	      if (tf->_last_h)
		  tf->_last_h_ready = true;
	      return 0;
	  } else
	      return errh->error("`extend_interval' takes a time interval");
      }
      default:
	return -EINVAL;
    }
}

void
TimeFilter::add_handlers()
{
    add_write_handler("extend_interval", write_handler, (void *)H_EXTEND_INTERVAL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimeFilter)
