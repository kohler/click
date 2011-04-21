// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * red.{cc,hh} -- element implements Random Early Detection dropping policy
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "adaptivered.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

AdaptiveRED::AdaptiveRED()
    : _timer(this)
{
}

AdaptiveRED::~AdaptiveRED()
{
}

void *
AdaptiveRED::cast(const char *n)
{
    if (strcmp(n, "RED") == 0 || strcmp(n, "AdaptiveRED") == 0)
	return (Element *)this;
    else
	return RED::cast(n);
}

int
AdaptiveRED::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned target_q, max_p, stability = 4;
    String queues_string = String();
    if (Args(conf, this, errh)
	.read_mp("TARGET", target_q)
	.read_mp("MAX_P", FixedPointArg(16), max_p)
	.read("QUEUES", AnyArg(), queues_string)
	.read("STABILITY", stability)
	.complete() < 0)
	return -1;
    if (target_q < 10)
	target_q = 10;
    unsigned min_thresh = target_q / 2;
    unsigned max_thresh = target_q + min_thresh;
    return finish_configure(min_thresh, max_thresh, true, max_p, stability, queues_string, errh);
}

int
AdaptiveRED::initialize(ErrorHandler *errh)
{
    if (RED::initialize(errh) < 0)
	return -1;

    _timer.initialize(this);
    _timer.schedule_after_msec(ADAPTIVE_INTERVAL);
    return 0;
}

void
AdaptiveRED::run_timer(Timer *)
{
    uint32_t avg;
    if (_size.stability_shift() == 0)
	avg = queue_size();	// use instantaneous measurement
    else
	avg = _size.unscaled_average();

    uint32_t part = (_max_thresh - _min_thresh) / 2;
    if (avg < _min_thresh + part && _max_p > ONE_HUNDREDTH) {
	_max_p = (_max_p * NINE_TENTHS) >> 16;
    } else if (avg > _max_thresh - part && _max_p < 0x8000) {
	uint32_t alpha = ONE_HUNDREDTH;
	if (alpha > _max_p/4)
	    alpha = _max_p/4;
	_max_p = _max_p + alpha;
    }
    set_C1_and_C2();
    _timer.reschedule_after_msec(ADAPTIVE_INTERVAL);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RED)
EXPORT_ELEMENT(AdaptiveRED)
