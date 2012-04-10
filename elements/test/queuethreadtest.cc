// -*- c-basic-offset: 4 -*-
/*
 * queuethreadtest.{cc,hh} -- regression test element for Queue threading
 * Eddie Kohler
 *
 * Copyright (c) 2007 Regents of the University of California
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
#include "queuethreadtest.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

QueueThreadTest1::QueueThreadTest1()
{
}

extern "C" {
static void *queue_thread_pusher(void *arg)
{
    SimpleQueue *sq = static_cast<SimpleQueue *>(arg);
    while (!sq->router()->running())
	/* do nothing */;

    uint32_t value = 0;
    Packet *p = Packet::make(4);

    while (1) {
	WritablePacket *q = p->uniqueify();
	*reinterpret_cast<uint32_t *>(q->data()) = value;
	int before_drops = sq->drops();
	sq->push(0, q->clone());
	if (sq->drops() == before_drops)
	    value++;
	p = q;
    }

    return 0;
}
}

int
QueueThreadTest1::initialize(ErrorHandler *errh)
{
    SimpleQueue *sq = static_cast<SimpleQueue *>(output(0).element());
    if (!sq)
	return errh->error("downstream element must be a type of Queue");
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int err = pthread_create(&_push_thread, &attr, queue_thread_pusher, sq);
    if (err != 0)
	return errh->error("cannot start thread: %s", strerror(err));
    else
	return 0;
}

void
QueueThreadTest1::cleanup(CleanupStage stage)
{
    if (stage >= CLEANUP_INITIALIZED)
	pthread_cancel(_push_thread);
}


QueueThreadTest2::QueueThreadTest2()
    : _task(this)
{
}

int
QueueThreadTest2::initialize(ErrorHandler *)
{
    _task.initialize(this, true);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    _next = _last_msg = 0;
    return 0;
}

#define CHECK(x) if (!(x)) errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);

bool
QueueThreadTest2::run_task(Task *)
{
    ErrorHandler *errh = ErrorHandler::default_handler();
    int i;
    for (i = 0; i < 100; i++)
	if (Packet *p = input(0).pull()) {
	    CHECK(p->length() == 4);
	    CHECK(* reinterpret_cast<const uint32_t *>(p->data()) == _next);
	    p->kill();
	    _next++;
	} else
	    break;
    if (static_cast<int32_t>(_last_msg + 1000000 - _next) < 0) {
	errh->message("%d tests succeeded...", _next);
	_last_msg += 1000000;
    }
    //    if (i != 0 || _signal)
    _task.fast_reschedule();
    return true;
}

ELEMENT_REQUIRES(userlevel umultithread)
EXPORT_ELEMENT(QueueThreadTest1 QueueThreadTest2)
CLICK_ENDDECLS
