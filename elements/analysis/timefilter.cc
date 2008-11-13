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
    : _last_h(0)
{
}

TimeFilter::~TimeFilter()
{
    delete _last_h;
}

int
TimeFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Timestamp first, last, first_init, last_init, first_delta, last_delta, interval;
    bool stop = false;

    if (cp_va_kparse(conf, this, errh,
		     "START", 0, cpTimestamp, &first,
		     "END", 0, cpTimestamp, &last,
		     "START_DELAY", 0, cpTimestamp, &first_init,
		     "END_DELAY", 0, cpTimestamp, &last_init,
		     "START_AFTER", 0, cpTimestamp, &first_delta,
		     "END_AFTER", 0, cpTimestamp, &last_delta,
		     "INTERVAL", 0, cpTimestamp, &interval,
		     "STOP", 0, cpBool, &stop,
		     "END_CALL", 0, cpHandlerCallPtrWrite, &_last_h,
		     cpEnd) < 0)
	return -1;

    _first_relative = _first_init_relative = _last_relative = _last_init_relative = _last_interval = false;

    if ((bool) first + (bool) first_delta + (bool) first_init > 1)
	return errh->error("'START', 'START_AFTER', and 'START_AFTER_INIT' are mutually exclusive");
    else if (first)
	_first = first;
    else if (first_init)
	_first = first_init, _first_init_relative = true;
    else
	_first = first_delta, _first_relative = true;

    if ((bool) last + (bool) last_delta + (bool) last_init + (bool) interval > 1)
	return errh->error("'END', 'END_AFTER', 'END_AFTER_INIT', and 'INTERVAL'\nare mutually exclusive");
    else if (last)
	_last = last;
    else if (last_delta)
	_last = last_delta, _last_relative = true;
    else if (last_init)
	_last = last_init, _last_init_relative = true;
    else if (interval)
	_last = interval, _last_interval = true;
    else
	_last.set_sec(0x7FFFFFFF);

    if (_last_h && stop)
	return errh->error("'END_CALL' and 'STOP' are mutually exclusive");
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
	Timestamp now = Timestamp::now();
	if (_first_init_relative)
	    _first += now;
	if (_last_init_relative)
	    _last += now;
    }
    _last_h_ready = (_last_h != 0);
    return 0;
}

void
TimeFilter::first_packet(const Timestamp& tv)
{
    if (_first_relative)
	_first += tv;
    if (_last_relative)
	_last += tv;
    else if (_last_interval)
	_last += _first;
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
    const Timestamp& tv = p->timestamp_anno();
    if (unlikely(!_ready))
	first_packet(tv);
    if (unlikely(tv < _first))
	return kill(p);
    else {
	if (!likely(tv < _last) && _last_h && _last_h_ready) {
	    _last_h_ready = false;
	    (void) _last_h->call_write();
	}
	if (!likely(tv < _last))
	    return kill(p);
	else
	    return p;
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
	  Timestamp t;
	  if (cp_time(s, &t)) {
	      tf->_last += t;
	      if (tf->_last_h)
		  tf->_last_h_ready = true;
	      return 0;
	  } else
	      return errh->error("'extend_interval' takes a time interval");
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
