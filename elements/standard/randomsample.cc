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
#include <click/confparse.hh>
#include <click/error.hh>

RandomSample::RandomSample()
    : Element(1, 1)
{
    MOD_INC_USE_COUNT;
}

RandomSample::~RandomSample()
{
    MOD_DEC_USE_COUNT;
}

void
RandomSample::notify_noutputs(int n)
{
    set_noutputs(n < 2 ? 1 : 2);
}

int
RandomSample::configure(Vector<String> &conf, ErrorHandler *errh)
{
    uint32_t sampling_prob = 0xFFFFFFFFU;
    uint32_t drop_prob = 0xFFFFFFFFU;
    bool active = true;
    if (cp_va_parse(conf, this, errh,
		    cpOptional,
		    cpUnsignedReal2, "sampling probability", SAMPLING_SHIFT, &sampling_prob,
		    cpKeywords,
		    "SAMPLE", cpUnsignedReal2, "sampling probability", SAMPLING_SHIFT, &sampling_prob,
		    "DROP", cpUnsignedReal2, "drop probability", SAMPLING_SHIFT, &drop_prob,
		    "ACTIVE", cpBool, "active?", &active,
		    0) < 0)
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

void
RandomSample::configuration(Vector<String> &conf, bool *) const
{
    conf.push_back("SAMPLE " + cp_unparse_real2(_sampling_prob, SAMPLING_SHIFT));
    conf.push_back("ACTIVE " + cp_unparse_bool(_active));
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
    if (!_active || (uint32_t)(random() & SAMPLING_MASK) < _sampling_prob)
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
    else if (!_active || (uint32_t)(random() & SAMPLING_MASK) < _sampling_prob)
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
    switch ((int)thunk) {
      case 0:
	return cp_unparse_real2(rs->_sampling_prob, SAMPLING_SHIFT) + "\n";
      case 1:
	return cp_unparse_bool(rs->_active) + "\n";
      case 2:
	return String(rs->_drops) + "\n";
      case 3:
	return cp_unparse_real2((1 << SAMPLING_SHIFT) - rs->_sampling_prob, SAMPLING_SHIFT) + "\n";
      default:
	return "<error>\n";
    }
}

void
RandomSample::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, (void *)0);
    add_write_handler("sampling_prob", reconfigure_keyword_handler, (void *)"SAMPLE");
    add_read_handler("active", read_handler, (void *)1);
    add_write_handler("active", reconfigure_keyword_handler, (void *)"ACTIVE");
    add_read_handler("drops", read_handler, (void *)2);
    add_read_handler("drop_prob", read_handler, (void *)3);
    add_write_handler("drop_prob", reconfigure_keyword_handler, (void *)"DROP");
}

EXPORT_ELEMENT(RandomSample)
ELEMENT_MT_SAFE(RandomSample)
