/*
 * RandomSource.{cc,hh} -- element generates random infinite stream
 * of packets
 * Ian Rose, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "randomsource.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/handlercall.hh>
CLICK_DECLS

RandomSource::RandomSource()
{
}

int
RandomSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
    ActiveNotifier::initialize(Notifier::EMPTY_NOTIFIER, router());
    counter_t limit = -1;
    int burstsize = 1;
    int datasize = -1;
    bool active = true, stop = false, timestamp = true;
    HandlerCall end_h;

    if (Args(conf, this, errh)
	.read_mp("LENGTH", datasize)
	.read_p("LIMIT", limit)
	.read_p("BURST", burstsize)
	.read_p("ACTIVE", active)
	.read("TIMESTAMP", timestamp)
	.read("STOP", stop)
	.read("END_CALL", HandlerCallArg(HandlerCall::writable), end_h)
	.complete() < 0)
	return -1;
    if (datasize < 0 || datasize >= 64*1024)
	return errh->error("bad length %d", datasize);
    if (burstsize < 1)
	return errh->error("burst size must be >= 1");
    if (stop && end_h)
	return errh->error("END_CALL and STOP are mutually exclusive");

    _datasize = datasize;
    _limit = limit;
    _burstsize = burstsize;
    _count = 0;
    _active = active;
    _timestamp = timestamp;
    delete _end_h;
    if (end_h)
	_end_h = new HandlerCall(end_h);
    else if (stop)
	_end_h = new HandlerCall("stop");
    else
	_end_h = 0;

    return 0;
}

bool
RandomSource::run_task(Task *)
{
    if (!_active || !_nonfull_signal)
	return false;
    int n = _burstsize;
    if (_limit >= 0 && _count + n >= (ucounter_t) _limit)
	n = (_count > (ucounter_t) _limit ? 0 : _limit - _count);
    for (int i = 0; i < n; i++) {
	Packet *p = make_packet();
	output(0).push(p);
    }
    _count += n;
    if (n > 0)
	_task.fast_reschedule();
    else if (_end_h && _limit >= 0 && _count >= (ucounter_t) _limit)
	(void) _end_h->call_write();
    return n > 0;
}

Packet *
RandomSource::pull(int)
{
    if (!_active) {
    done:
	if (Notifier::active())
	    sleep();
	return 0;
    }
    if (_limit >= 0 && _count >= (ucounter_t) _limit) {
	if (_end_h)
	    (void) _end_h->call_write();
	goto done;
    }
    _count++;
    Packet *p = make_packet();
    return p;
}

Packet *
RandomSource::make_packet()
{
    WritablePacket *p = Packet::make(36, (const unsigned char*)0, _datasize, 0);

    int i;
    char *d = (char *) p->data();
    for (i = 0; i < _datasize; i += sizeof(int))
	*(int*)(d + i) = click_random();
    for( ; i < _datasize; i++)
	*(d + i) = click_random();

    if (_timestamp)
	p->timestamp_anno().assign_now();
    return p;
}

void
RandomSource::add_handlers()
{
    add_data_handlers("limit", Handler::OP_READ | Handler::CALM, &_limit);
    add_write_handler("limit", change_param, h_limit);
    add_data_handlers("burst", Handler::OP_READ | Handler::CALM, &_burstsize);
    add_write_handler("burst", change_param, h_burst);
    add_data_handlers("active", Handler::OP_READ | Handler::CHECKBOX, &_active);
    add_write_handler("active", change_param, h_active);
    add_data_handlers("count", Handler::OP_READ, &_count);
    add_write_handler("reset", change_param, h_reset, Handler::BUTTON);
    add_data_handlers("length", Handler::OP_READ | Handler::CALM, &_datasize);
    add_write_handler("length", change_param, h_length);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RandomSource)
ELEMENT_MT_SAFE(RandomSource)
