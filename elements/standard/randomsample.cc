// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * randomsample.{cc,hh} -- element probabilistically samples packets
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
#include "randomsample.hh"
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
CLICK_DECLS

RandomSample::RandomSample()
{
}

RandomSample::~RandomSample()
{
}

int
RandomSample::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t sampling_prob = 0xFFFFFFFFU;
    uint32_t drop_prob = 0xFFFFFFFFU;
    bool active = true;
    if (Args(conf, this, errh)
	.read_p("P", FixedPointArg(SAMPLING_SHIFT), sampling_prob)
	.read("SAMPLE", FixedPointArg(SAMPLING_SHIFT), sampling_prob)
	.read("DROP", FixedPointArg(SAMPLING_SHIFT), drop_prob)
	.read("ACTIVE", active)
	.complete() < 0)
	return -1;
    if (sampling_prob == 0xFFFFFFFFU && drop_prob <= (1 << SAMPLING_SHIFT))
	sampling_prob = (1 << SAMPLING_SHIFT) - drop_prob;
    if (sampling_prob > (1 << SAMPLING_SHIFT))
	return errh->error("sampling probability must be between 0 and 1");

    // OK: set variables
    _sampling_prob = sampling_prob;
    _active = active;

    return 0;
}

int
RandomSample::initialize(ErrorHandler *)
{
    _drops = 0;
    return 0;
}

void
RandomSample::push(int, Packet *p)
{
    if (!_active || (click_random() & SAMPLING_MASK) < _sampling_prob)
	output(0).push(p);
    else {
	checked_output_push(1, p);
	_drops++;
    }
}

Packet *
RandomSample::pull(int)
{
    Packet *p = input(0).pull();
    if (!p)
	return 0;
    else if (!_active || (click_random() & SAMPLING_MASK) < _sampling_prob)
	return p;
    else {
	checked_output_push(1, p);
	_drops++;
	return 0;
    }
}

String
RandomSample::read_handler(Element *e, void *thunk)
{
    RandomSample *rs = static_cast<RandomSample *>(e);
    switch ((intptr_t)thunk) {
      case 0:
	return cp_unparse_real2(rs->_sampling_prob, SAMPLING_SHIFT);
      case 3:
	return cp_unparse_real2((1 << SAMPLING_SHIFT) - rs->_sampling_prob, SAMPLING_SHIFT);
      case 4: {
	  StringAccum sa;
	  sa << "SAMPLE " << cp_unparse_real2(rs->_sampling_prob, SAMPLING_SHIFT)
	     << ", ACTIVE " << rs->_active;
	  return sa.take_string();
      }
      default:
	return "<error>";
    }
}

void
RandomSample::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, 0);
    add_write_handler("sampling_prob", reconfigure_keyword_handler, (void *)"SAMPLE");
    add_data_handlers("active", Handler::OP_READ | Handler::CHECKBOX, &_active);
    add_write_handler("active", reconfigure_keyword_handler, "ACTIVE");
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_read_handler("drop_prob", read_handler, 3);
    add_write_handler("drop_prob", reconfigure_keyword_handler, "DROP");
    add_read_handler("config", read_handler, 4);
    set_handler_flags("config", 0, Handler::CALM);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RandomSample)
ELEMENT_MT_SAFE(RandomSample)
