// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdagdump.{cc,hh} -- element reads packets from DAG (Waikato) file
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
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
#include "fromdagdump.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/handlercall.hh>
#include <clicknet/rfc1483.h>
#include <click/userutils.hh>
#include "elements/userlevel/fakepcap.hh"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef ALLOW_MMAP
# include <sys/mman.h>
#endif
CLICK_DECLS

#define	SWAPLONG(y) \
	((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))
#define	SWAPSHORT(y) \
	( (((y)&0xff)<<8) | ((u_short)((y)&0xff00)>>8) )

FromDAGDump::FromDAGDump()
    : Element(0, 1), _packet(0), _last_time_h(0), _task(this)
{
    MOD_INC_USE_COUNT;
    static_assert(sizeof(DAGCell) == 64);
    static_assert(sizeof(((DAGCell *) 0)->payload) == sizeof(DAGCell) - DAGCell::PAYLOAD_OFFSET);
}

FromDAGDump::~FromDAGDump()
{
    MOD_DEC_USE_COUNT;
    delete _last_time_h;
}

void
FromDAGDump::notify_noutputs(int n)
{
    set_noutputs(n <= 1 ? 1 : 2);
}

int
FromDAGDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool timing = false, stop = false, active = true, force_ip = false;
    struct timeval first_time, first_time_off, last_time, last_time_off, interval;
    timerclear(&first_time);
    timerclear(&first_time_off);
    timerclear(&last_time);
    timerclear(&last_time_off);
    timerclear(&interval);
    _sampling_prob = (1 << SAMPLING_SHIFT);

    if (_ff.configure_keywords(conf, 1, this, errh) < 0)
	return -1;
    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump file name", &_ff.filename(),
		    cpKeywords,
		    "STOP", cpBool, "stop driver when done?", &stop,
		    "ACTIVE", cpBool, "start active?", &active,
		    "FORCE_IP", cpBool, "emit IP packets only?", &force_ip,
		    "START", cpTimeval, "starting time", &first_time,
		    "START_AFTER", cpTimeval, "starting time offset", &first_time_off,
		    "END", cpTimeval, "ending time", &last_time,
		    "END_AFTER", cpTimeval, "ending time offset", &last_time_off,
		    "INTERVAL", cpTimeval, "time interval", &interval,
		    "END_CALL", cpWriteHandlerCall, "write handler for ending time", &_last_time_h,
		    "SAMPLE", cpUnsignedReal2, "sampling probability", SAMPLING_SHIFT, &_sampling_prob,
		    "TIMING", cpBool, "use original packet timing?", &timing,
		    cpEnd) < 0)
	return -1;

    // check sampling rate
    if (_sampling_prob > (1 << SAMPLING_SHIFT)) {
	errh->warning("SAMPLE probability reduced to 1");
	_sampling_prob = (1 << SAMPLING_SHIFT);
    } else if (_sampling_prob == 0)
	errh->warning("SAMPLE probability is 0; emitting no packets");

    // check times
    _have_first_time = _have_last_time = true;
    _first_time_relative = _last_time_relative = _last_time_interval = false;
    
    if ((timerisset(&first_time) != 0) + (timerisset(&first_time_off) != 0) > 1)
	return errh->error("'START' and 'START_AFTER' are mutually exclusive");
    else if (timerisset(&first_time))
	_first_time = first_time;
    else if (timerisset(&first_time_off))
	_first_time = first_time_off, _first_time_relative = true;
    else {
	timerclear(&_first_time);
	_have_first_time = false, _first_time_relative = true;
    }
    
    if ((timerisset(&last_time) != 0) + (timerisset(&last_time_off) != 0) + (timerisset(&interval) != 0) > 1)
	return errh->error("'END', 'END_AFTER', and 'INTERVAL' are mutually exclusive");
    else if (timerisset(&last_time))
	_last_time = last_time;
    else if (timerisset(&last_time_off))
	_last_time = last_time_off, _last_time_relative = true;
    else if (timerisset(&interval))
	_last_time = interval, _last_time_interval = true;
    else
	_have_last_time = false;

    if (_have_last_time && !_last_time_h)
	_last_time_h = new HandlerCall(id() + ".active false");
    
    // set other variables
    _have_any_times = false;
    _timing = timing;
    _stop = stop;
    _force_ip = force_ip;
    _linktype = FAKE_DLT_ATM_RFC1483;
    _active = active;
    return 0;
}

int
FromDAGDump::initialize(ErrorHandler *errh)
{
    if (_ff.initialize(errh) < 0)
	return -1;
    
    // if forcing IP packets, check datalink type to ensure we understand it
    if (_force_ip) {
	if (!fake_pcap_dlt_force_ipable(_linktype))
	    return _ff.error(errh, "unknown linktype %d; can't force IP packets", _linktype);
	if (_timing)
	    return errh->error("FORCE_IP and TIMING options are incompatible");
    }

    // check handler call
    if (_last_time_h && _last_time_h->initialize_write(this, errh) < 0)
	return -1;
    
    // try reading a packet
    if (read_packet(errh)) {
	struct timeval now;
	click_gettimeofday(&now);
	timersub(&now, &_packet->timestamp_anno(), &_time_offset);
    }

    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return 0;
}

void
FromDAGDump::cleanup(CleanupStage)
{
    _ff.cleanup();
    if (_packet)
	_packet->kill();
    _packet = 0;
}

void
FromDAGDump::set_active(bool active)
{
    if (_active != active) {
	_active = active;
	if (active && output_is_push(0) && !_task.scheduled())
	    _task.reschedule();
    }
}

static inline uint64_t
swapq(uint64_t q)
{
#if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    return ((q & 0xff00000000000000LL) >> 56)
	| ((q & 0x00ff000000000000LL) >> 40)
	| ((q & 0x0000ff0000000000LL) >> 24)
	| ((q & 0x000000ff00000000LL) >>  8)
	| ((q & 0x00000000ff000000LL) <<  8)
	| ((q & 0x0000000000ff0000LL) << 24)
	| ((q & 0x000000000000ff00LL) << 40)
	| ((q & 0x00000000000000ffLL) << 56);
#elif CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    return q;
#else
#error "neither big nor little endian"
#endif
}

void
FromDAGDump::stamp_to_timeval(uint64_t stamp, struct timeval &tv) const
{
    tv.tv_sec = (uint32_t) (stamp >> 32);
    tv.tv_usec = (uint32_t) ((stamp * 1000000) >> 32);
}

void
FromDAGDump::prepare_times(struct timeval &tv)
{
    if (_first_time_relative)
	timeradd(&tv, &_first_time, &_first_time);
    if (_last_time_relative)
	timeradd(&tv, &_last_time, &_last_time);
    else if (_last_time_interval)
	timeradd(&_first_time, &_last_time, &_last_time);
    _have_any_times = true;
}

bool
FromDAGDump::read_packet(ErrorHandler *errh)
{
    const DAGCell *cell;
    static DAGCell static_cell;
    struct timeval tv;
    Packet *p;
    bool more = true;
    _packet = 0;

  retry:
    // quit if we sampled or force_ip failed, but we are no longer active
    if (!more)
	return false;
    
    // we may need to read bits of the file
    cell = reinterpret_cast<const DAGCell *>(_ff.get_aligned(sizeof(DAGCell), &static_cell, errh));
    if (!cell)
	return false;

    // check times
  check_times:
    stamp_to_timeval(swapq(cell->timestamp), tv);
    if (!_have_any_times)
	prepare_times(tv);
    if (_have_first_time) {
	if (timercmp(&tv, &_first_time, <))
	    goto retry;
	else
	    _have_first_time = false;
    }
    if (_have_last_time && !timercmp(&tv, &_last_time, <)) {
	_have_last_time = false;
	(void) _last_time_h->call_write(errh);
	if (!_active)
	    more = false;
	// The handler might have scheduled us, in which case we might crash
	// at fast_reschedule()! Don't want that -- make sure we are
	// unscheduled.
	_task.fast_unschedule();
	// retry _last_time in case someone changed it
	goto check_times;
    }
    
    // checking sampling probability
    if (_sampling_prob < (1 << SAMPLING_SHIFT)
	&& (uint32_t)(random() & ((1<<SAMPLING_SHIFT)-1)) >= _sampling_prob)
	goto retry;
    
    // create packet
    p = _ff.get_packet_from_data(&cell->payload, sizeof(cell->payload), tv.tv_sec, tv.tv_usec, errh);
    if (!p)
	return false;

    if (_force_ip && !fake_pcap_force_ip(p, _linktype)) {
	checked_output_push(1, p);
	goto retry;
    }

    _packet = p;
    return more;
}

bool
FromDAGDump::run_task()
{
    if (!_active)
	return false;

    bool more;
    if (_packet || read_packet(0)) {
	if (_timing) {
	    struct timeval now;
	    click_gettimeofday(&now);
	    timersub(&now, &_time_offset, &now);
	    if (timercmp(&_packet->timestamp_anno(), &now, >)) {
		_task.fast_reschedule();
		return false;
	    }
	}
	output(0).push(_packet);
	more = read_packet(0);
    } else
	more = false;

    if (more)
	_task.fast_reschedule();
    else if (_stop)
	router()->please_stop_driver();
    return true;
}

Packet *
FromDAGDump::pull(int)
{
    if (!_active)
	return 0;

    bool more;
    Packet *p;
    if (_packet || read_packet(0)) {
	if (_timing) {
	    struct timeval now;
	    click_gettimeofday(&now);
	    timersub(&now, &_time_offset, &now);
	    if (timercmp(&_packet->timestamp_anno(), &now, >))
		return 0;
	}
	p = _packet;
	more = read_packet(0);
    } else {
	p = 0;
	more = false;
    }

    if (!more && _stop)
	router()->please_stop_driver();
    return p;
}

enum {
    H_SAMPLING_PROB, H_ACTIVE, H_ENCAP, H_STOP, H_EXTEND_INTERVAL
};

String
FromDAGDump::read_handler(Element *e, void *thunk)
{
    FromDAGDump *fd = static_cast<FromDAGDump *>(e);
    switch ((intptr_t)thunk) {
      case H_SAMPLING_PROB:
	return cp_unparse_real2(fd->_sampling_prob, SAMPLING_SHIFT) + "\n";
      case H_ACTIVE:
	return cp_unparse_bool(fd->_active) + "\n";
      case H_ENCAP:
	return String(fake_pcap_unparse_dlt(fd->_linktype)) + "\n";
      default:
	return "<error>\n";
    }
}

int
FromDAGDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromDAGDump *fd = static_cast<FromDAGDump *>(e);
    String s = cp_uncomment(s_in);
    switch ((intptr_t)thunk) {
      case H_ACTIVE: {
	  bool active;
	  if (cp_bool(s, &active)) {
	      fd->set_active(active);
	      return 0;
	  } else
	      return errh->error("'active' should be Boolean");
      }
      case H_STOP:
	fd->set_active(false);
	fd->router()->please_stop_driver();
	return 0;
      case H_EXTEND_INTERVAL: {
	  struct timeval tv;
	  if (cp_timeval(s, &tv)) {
	      timeradd(&fd->_last_time, &tv, &fd->_last_time);
	      if (fd->_last_time_h)
		  fd->_have_last_time = true, fd->set_active(true);
	      return 0;
	  } else
	      return errh->error("'extend_interval' takes a time interval");
      }
      default:
	return -EINVAL;
    }
}

void
FromDAGDump::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, (void *)H_SAMPLING_PROB);
    add_read_handler("active", read_handler, (void *)H_ACTIVE);
    add_write_handler("active", write_handler, (void *)H_ACTIVE);
    add_read_handler("encap", read_handler, (void *)H_ENCAP);
    add_write_handler("stop", write_handler, (void *)H_STOP);
    add_write_handler("extend_interval", write_handler, (void *)H_EXTEND_INTERVAL);
    _ff.add_handlers(this);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel int64 FakePcap FromFile)
EXPORT_ELEMENT(FromDAGDump)
