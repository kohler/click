// -*- c-basic-offset: 4 -*-
/*
 * linkunqueue.{cc,hh} -- element pulls packets from input, delaying them as
 * if they had passed through a serial link.
 * Eddie Kohler
 * (inspired partially by SerialLink, by Tao Zhao and Eric Freudenthal
 * from NYU)
 *
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/confparse.hh>
#include <click/glue.hh>
#include "linkunqueue.hh"
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

LinkUnqueue::LinkUnqueue()
    : Element(1, 1), _qhead(0), _qtail(0), _task(this), _timer(&_task)
{
    MOD_INC_USE_COUNT;
}

LinkUnqueue::~LinkUnqueue()
{
    MOD_DEC_USE_COUNT;
}

void *
LinkUnqueue::cast(const char *n)
{
    if (strcmp(n, "Storage") == 0)
	return (Storage *)this;
    else
	return Element::cast(n);
}

int
LinkUnqueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_parse(conf, this, errh,
		    cpInterval, "latency", &_latency,
		    cpBandwidth, "bandwidth", &_bandwidth,
		    cpEnd) < 0)
	return -1;
    if (_bandwidth < 100)
	return errh->error("bandwidth too small, minimum 100Bps");
    _bandwidth /= 100;
    return 0;
}

int
LinkUnqueue::initialize(ErrorHandler *errh)
{
    ScheduleInfo::initialize_task(this, &_task, errh);
    _timer.initialize(this);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    Storage::_capacity = 0x7FFFFFFF;
    _state = S_ASLEEP;
    return 0;
}

void
LinkUnqueue::cleanup(CleanupStage)
{
    while (_qhead) {
	Packet *p = _qhead;
	_qhead = p->next();
	p->kill();
    }
}

void
LinkUnqueue::delay_by_bandwidth(Packet *p, const Timestamp &tv) const
{
    uint32_t length = p->length() + EXTRA_LENGTH_ANNO(p);
    uint32_t delay = (length * 10000) / _bandwidth;
    Timestamp &timestamp = p->timestamp_anno();
    timestamp = tv;
    if (delay >= 1000000) {
	timestamp._sec += delay / 1000000;
	timestamp._subsec += Timestamp::usec_to_subsec(delay % 1000000);
    } else
	timestamp._subsec += Timestamp::usec_to_subsec(delay);
    timestamp.add_fix();
}

#if 0
#include <click/straccum.hh>
static void print_queue(Packet *head) {
    static Timestamp first;
    if (!first && head)
	first = head->timestamp_anno();
    StringAccum sa;
    sa << '[';
    while (head) {
	Timestamp diff = head->timestamp_anno() - first;
	sa << ' ' << diff;
	head = head->next();
    }
    sa << ' ' << ']' << ' ' << '@' << (Timestamp::now() - first);
    click_chatter("%s", sa.c_str());
}
#endif

bool
LinkUnqueue::run_task()
{
    bool worked = false;
    Timestamp now = Timestamp::now();

    // Read a new packet if there's room
    if (_signal) {
	Timestamp now_delayed = now + _latency;
	
	// check for timer problems
	if (_state == S_TIMER && _qtail
	    && now_delayed >= _qtail->timestamp_anno())
	    _state = S_TASK;
	
	Packet *p;
	while ((!_qtail || now_delayed >= _qtail->timestamp_anno())
	       && (p = input(0).pull())) {
	    if (_qtail) {
		_qtail->set_next(p);
		if ((worked || _state == S_TASK) && _qtail)
		    delay_by_bandwidth(p, _qtail->timestamp_anno());
		else
		    delay_by_bandwidth(p, now_delayed);
		//click_chatter("%{timestamp}: %d GOT NEW %{timestamp}", &now, _state, &_qtail->timestamp_anno());
	    } else {
		_qhead = p;
		delay_by_bandwidth(p, now_delayed);
	    }
	    _qtail = p;
	    p->set_next(0);
	    Storage::_tail++;
	    worked = true;
	}
    }

    // Emit packets if it's time
    while (_qhead && now >= _qhead->timestamp_anno()) {
	Packet *p = _qhead;
	_qhead = p->next();
	if (!_qhead)
	    _qtail = 0;
	p->set_next(0);
	//click_chatter("%{timestamp}: RELEASE %{timestamp}", &now, &p->timestamp_anno());
	output(0).push(p);
	Storage::_tail--;
	worked = true;
    }

    // Figure out when to schedule next
    //print_queue(_qhead);
    if (_qhead) {
	now = Timestamp::now();
	Timestamp expiry = _qhead->timestamp_anno();
	if (_signal) {
	    Timestamp expiry2 = _qtail->timestamp_anno() - _latency;
	    if (expiry2 < expiry)
		expiry = expiry2;
	}
	//{ Timestamp diff = expiry - now; click_chatter("%{timestamp}: %{timestamp} > + %{timestamp}", &now, &expiry, &diff); }
	expiry -= Timestamp(0, 5000);
	if (expiry <= now) {
	    // small delay, reschedule Task
	    _state = S_TASK;
	    _task.fast_reschedule();
	} else {
	    // large delay, schedule Timer instead
	    _state = S_TIMER;
	    _timer.schedule_at(expiry);
	}
    } else if (_signal) {
	_state = S_TASK;
	_task.fast_reschedule();
    } else
	_state = S_ASLEEP;

    //click_chatter("\n-> %d", _state);
    return worked;
}

enum { H_LATENCY, H_BANDWIDTH, H_SIZE };

String
LinkUnqueue::read_param(Element *e, void *thunk)
{
    LinkUnqueue *u = (LinkUnqueue *)e;
    switch ((intptr_t) thunk) {
      case H_LATENCY:
	return cp_unparse_interval(u->_latency) + "\n";
      case H_BANDWIDTH:
	return String(u->_bandwidth * 100) + "\n";
      case H_SIZE:
	return String(u->Storage::size()) + "\n";
      default:
	return "<error>\n";
    }
}

int
LinkUnqueue::write_handler(const String &, Element *e, void *, ErrorHandler *)
{
    LinkUnqueue *u = (LinkUnqueue *)e;
    u->cleanup(CLEANUP_MANUAL);
    u->_qhead = u->_qtail = 0;
    u->Storage::_tail = 0;
    u->_timer.unschedule();
    u->_task.reschedule();
    return 0;
}

void
LinkUnqueue::add_handlers()
{
    add_read_handler("latency", read_param, (void *)H_LATENCY);
    add_read_handler("bandwidth", read_param, (void *)H_BANDWIDTH);
    add_read_handler("size", read_param, (void *)H_SIZE);
    add_write_handler("reset", write_handler, 0);
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LinkUnqueue)
