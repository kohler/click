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

TimeFilter::TimeFilter()
    : Element(1, 1)
{
    MOD_INC_USE_COUNT;
}

TimeFilter::~TimeFilter()
{
    MOD_DEC_USE_COUNT;
}

void
TimeFilter::notify_noutputs(int n)
{
    set_noutputs(n <= 1 ? 1 : 2);
}

int
TimeFilter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
    struct timeval first, last, first_delta, last_delta, interval;
    timerclear(&first);
    timerclear(&last);
    timerclear(&first_delta);
    timerclear(&last_delta);
    timerclear(&interval);
    bool stop = false;

    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "START", cpTimeval, "start time", &first,
		    "END", cpTimeval, "end time", &last,
		    "START_AFTER", cpTimeval, "start after", &first_delta,
		    "END_AFTER", cpTimeval, "end after", &last_delta,
		    "INTERVAL", cpTimeval, "interval", &interval,
		    "STOP", cpBool, "stop when after end?", &stop,
		    0) < 0)
	return -1;

    if (timerisset(&first) && timerisset(&first_delta))
	return errh->error("`START' and `START_AFTER' are mutually exclusive");
    else if (timerisset(&first))
	_first = first, _first_relative = false;
    else
	_first = first_delta, _first_relative = true;
    
    if ((timerisset(&last) != 0) + (timerisset(&last_delta) != 0) + (timerisset(&interval) > 1))
	return errh->error("`END', `END_AFTER', and `INTERVAL' are mutually exclusive");
    else if (timerisset(&last))
	_last = last, _last_relative = false, _last_interval = false;
    else if (timerisset(&last_delta))
	_last = last_delta, _last_relative = true, _last_interval = false;
    else
	_last = interval, _last_relative = false, _last_interval = true;

    _ready = false;
    _stop = stop;
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
	if (_stop)
	    router()->please_stop_driver();
	return kill(p);
    }
}

EXPORT_ELEMENT(TimeFilter)
