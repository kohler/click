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
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

AdaptiveRED::AdaptiveRED()
    : _timer(this)
{
    MOD_INC_USE_COUNT;
}

AdaptiveRED::~AdaptiveRED()
{
    MOD_DEC_USE_COUNT;
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
    if (cp_va_parse(conf, this, errh,
		    cpUnsigned, "target queue length", &target_q,
		    cpUnsignedReal2, "max_p drop probability", 16, &max_p,
		    cpKeywords,
		    "QUEUES", cpArgument, "relevant queues", &queues_string,
		    "STABILITY", cpUnsigned, "stability shift", &stability,
		    0) < 0)
	return -1;
    if (target_q < 10)
	target_q = 10;
    unsigned min_thresh = target_q / 2;
    unsigned max_thresh = target_q + min_thresh;
    return finish_configure(min_thresh, max_thresh, max_p, stability, queues_string, errh);
}

int
AdaptiveRED::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned target_q, max_p, stability = 4;
    String queues_string = String();
    if (cp_va_parse(conf, this, errh,
		    cpUnsigned, "target queue length", &target_q,
		    cpUnsignedReal2, "max_p drop probability", 16, &max_p,
		    cpKeywords,
		    "QUEUES", cpArgument, "relevant queues", &queues_string,
		    "STABILITY", cpUnsigned, "stability shift", &stability,
		    0) < 0)
	return -1;
    if (queues_string)
	errh->warning("QUEUES argument ignored");
    if (target_q < 10)
	target_q = 10;
    unsigned min_thresh = target_q / 2;
    unsigned max_thresh = target_q + min_thresh;
    return finish_configure(min_thresh, max_thresh, max_p, stability, String(), errh);
}

int
AdaptiveRED::initialize(ErrorHandler *errh)
{
    if (RED::initialize(errh) < 0)
	return -1;

    _timer.initialize(this);
    _timer.schedule_after_ms(ADAPTIVE_INTERVAL);
    return 0;
}

void
AdaptiveRED::cleanup(CleanupStage)
{
    _timer.cleanup();
}

void
AdaptiveRED::run_scheduled()
{
    uint32_t part = (_max_thresh - _min_thresh) / 2;
    uint32_t avg = (_size.average() >> QUEUE_SCALE);
    if (avg < _min_thresh + part && _max_p > ONE_HUNDREDTH) {
	_max_p = (_max_p * NINE_TENTHS) >> 16;
    } else if (avg > _max_thresh - part && _max_p < 0x8000) {
	uint32_t alpha = ONE_HUNDREDTH;
	if (alpha > _max_p/4)
	    alpha = _max_p/4;
	_max_p = _max_p + alpha;
    }
    set_C1_and_C2();
    _timer.reschedule_after_ms(ADAPTIVE_INTERVAL);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RED)
EXPORT_ELEMENT(AdaptiveRED)
