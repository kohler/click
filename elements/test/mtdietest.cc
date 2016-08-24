// -*- c-basic-offset: 4 -*-
/*
 * rcutest.{cc,hh} -- regression test element for click_rcu
 * Tom Barbette
 *
 * Copyright (c) 2016 Cisco Meraki
 * Copyright (c) 2016 University of Liege
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
#include "mtdietest.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/master.hh>
CLICK_DECLS

MTDieTest::MTDieTest()
    : _task()
{
}

int
MTDieTest::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_nthreads = master()->nthreads();
    if (Args(conf, this, errh)
	.read("NTHREADS", _nthreads)
	.complete() < 0)
	return -1;
    return 0;
}

int
MTDieTest::initialize(ErrorHandler *)
{
    _task.resize(_nthreads);
    for (int i = 0; i < _nthreads; i++) {
    	_task[i] = new Task(this);
        _task[i]->initialize(this, false);
        _task[i]->move_thread(i);
    	_task[i]->reschedule();
    }
    return 0;
}

bool
MTDieTest::run_task(Task *t)
{
	router()->please_stop_driver();
}


CLICK_ENDDECLS
EXPORT_ELEMENT(MTDieTest)
