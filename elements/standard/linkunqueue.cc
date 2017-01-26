// -*- c-basic-offset: 4 -*-
/*
 * linkunqueue.{cc,hh} -- element pulls packets from input, delaying them as
 * if they had passed through a serial link.
 * Eddie Kohler
 * (inspired partially by SerialLink, by Tao Zhao and Eric Freudenthal
 * from NYU)
 *
 * Copyright (c) 2003 International Computer Science Institute
 * Copyright (c) 2005-2006 Regents of the University of California
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
#include <click/args.hh>
#include <click/glue.hh>
#include "linkunqueue.hh"
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

LinkUnqueue::LinkUnqueue()
    : _qhead(0), _qtail(0), _task(this), _timer(&_task)
{
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
    if (Args(conf, this, errh)
	.read_mp("LATENCY", _latency)
	.read_mp("BANDWIDTH", BandwidthArg(), _bandwidth).complete() < 0)
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
    //_state = S_ASLEEP;
    _back_to_back = false;
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
    p->set_timestamp_anno(tv + Timestamp::make_usec(delay));
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
LinkUnqueue::run_task(Task *)
{
    bool worked = false;
    Timestamp now = Timestamp::now();
    Timestamp now_delayed = now + _latency;

    // Read a new packet if there's room.  Room is measured by the latency
    while (!_qtail || _qtail->timestamp_anno() <= now_delayed) {
	// try to pull a packet
	Packet *p = input(0).pull();
	if (!p) {
	    _back_to_back = false;
	    break;
	}

	// set new timestamp to delayed timestamp
	if (_qtail) {
	    _qtail->set_next(p);
	    delay_by_bandwidth(p, (_back_to_back ? _qtail->timestamp_anno() : now_delayed));
	} else {
	    _qhead = p;
	    delay_by_bandwidth(p, now_delayed);
	}

	// hook up, and remember we were doing this back to back
	_qtail = p;
	p->set_next(0);
	Storage::set_tail(Storage::tail() + 1);
	worked = _back_to_back = true;
    }

    // Emit packets if it's time
    while (_qhead && _qhead->timestamp_anno() <= now) {
	Packet *p = _qhead;
	_qhead = p->next();
	if (!_qhead)
	    _qtail = 0;
	p->set_next(0);
	//click_chatter("%p{timestamp}: RELEASE %p{timestamp}", &now, &p->timestamp_anno());
	output(0).push(p);
	Storage::set_tail(Storage::tail() - 1);
	worked = true;
    }

    // Figure out when to schedule next
    //print_queue(_qhead);
    if (_qhead) {
	Timestamp expiry = _qhead->timestamp_anno();
	if (_signal) {
	    Timestamp expiry2 = _qtail->timestamp_anno() - _latency;
	    if (expiry2 < expiry)
		expiry = expiry2;
	}
	//{ Timestamp diff = expiry - now; click_chatter("%p{timestamp}: %p{timestamp} > + %p{timestamp}", &now, &expiry, &diff); }
	expiry -= Timer::adjustment();
	if (expiry <= now) {
	    // small delay, reschedule Task
	    //_state = S_TASK;
	    _task.fast_reschedule();
	} else {
	    // large delay, schedule Timer instead
	    //_state = S_TIMER;
	    _timer.schedule_at(expiry);
	}
    } else if (_signal) {
	//_state = S_TASK;
	_task.fast_reschedule();
    } else {
	//_state = S_ASLEEP;
    }

    return worked;
}

enum { H_LATENCY, H_BANDWIDTH, H_SIZE, H_RESET };

String
LinkUnqueue::read_param(Element *e, void *thunk)
{
    LinkUnqueue *u = (LinkUnqueue *)e;
    switch ((intptr_t) thunk) {
      case H_LATENCY:
	return u->_latency.unparse_interval();
      case H_BANDWIDTH:
	return String(u->_bandwidth * 100);
      case H_SIZE:
	return String(u->Storage::size());
      default:
	return "<error>";
    }
}

int
LinkUnqueue::write_handler(const String &s, Element *e, void *thunk, ErrorHandler *errh)
{
    LinkUnqueue *u = (LinkUnqueue *)e;
    switch ((intptr_t) thunk) {
    case H_LATENCY: {
          Timestamp l;
          if (!cp_time(s, &l)) {
              return errh->error("latency must be a timestamp");
          }
          u->_latency = l;
          break;
    }
    case H_BANDWIDTH: {
        uint32_t bw;
        if (!cp_bandwidth(s, &bw)) {
            return errh->error("invalid bandwidth");
        } else if (bw < 100) {
            return errh->error("bandwidth too small, minimum 100Bps");
        }
        u->_bandwidth = bw / 100;
        break;
    }
    case H_RESET:
        break;
    default:
        return errh->error("unknown handler");
    }
    /* do a full reset */
    u->cleanup(CLEANUP_MANUAL);
    u->_qhead = u->_qtail = 0;
    u->Storage::set_tail(0);
    u->_timer.unschedule();
    u->_task.reschedule();
    return 0;
}

void
LinkUnqueue::add_handlers()
{
    add_read_handler("latency", read_param, H_LATENCY, Handler::CALM);
    add_read_handler("bandwidth", read_param, H_BANDWIDTH, Handler::CALM);
    add_read_handler("size", read_param, H_SIZE);
    add_write_handler("reset", write_handler, H_RESET, Handler::BUTTON);
    add_write_handler("latency", write_handler, H_LATENCY);
    add_write_handler("bandwidth", write_handler, H_BANDWIDTH);
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LinkUnqueue)
