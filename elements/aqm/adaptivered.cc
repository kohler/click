// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * red.{cc,hh} -- element implements Random Early Detection dropping policy
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 ACIRI
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

AdaptiveRED::AdaptiveRED()
    : _timer(this)
{
    MOD_INC_USE_COUNT;
}

AdaptiveRED::~AdaptiveRED()
{
    MOD_DEC_USE_COUNT;
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
AdaptiveRED::uninitialize()
{
    _timer.uninitialize();
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

ELEMENT_REQUIRES(RED)
EXPORT_ELEMENT(AdaptiveRED)
