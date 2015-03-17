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
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/handlercall.hh>
#include <click/packet_anno.hh>
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
    : _packet(0), _end_h(0), _task(this)
{
    static_assert(sizeof(DAGCell) == 64 && DAGCell::CELL_SIZE == 64, "DAGCell must be exactly 64 bytes long.");
}

FromDAGDump::~FromDAGDump()
{
    delete _end_h;
}

int
FromDAGDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool timing = false, stop = false, active = true, force_ip = false;
    Timestamp first_time, first_time_off, last_time, last_time_off, interval;
    HandlerCall end_h;
    String encap;
    _sampling_prob = (1 << SAMPLING_SHIFT);

    if (_ff.configure_keywords(conf, this, errh) < 0)
	return -1;
    if (Args(conf, this, errh)
	.read_mp("FILENAME", FilenameArg(), _ff.filename())
	.read("STOP", stop)
	.read("ACTIVE", active)
	.read("FORCE_IP", force_ip)
	.read("START", first_time)
	.read("START_AFTER", first_time_off)
	.read("END", last_time)
	.read("END_AFTER", last_time_off)
	.read("INTERVAL", interval)
	.read("END_CALL", HandlerCallArg(HandlerCall::writable), end_h)
	.read("SAMPLE", FixedPointArg(SAMPLING_SHIFT), _sampling_prob)
	.read("TIMING", timing)
	.read("ENCAP", WordArg(), encap)
	.complete() < 0)
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

    if ((bool) first_time + (bool) first_time_off > 1)
	return errh->error("'START' and 'START_AFTER' are mutually exclusive");
    else if ((bool) first_time)
	_first_time = first_time;
    else if ((bool) first_time_off)
	_first_time = first_time_off, _first_time_relative = true;
    else
	_have_first_time = false, _first_time_relative = true;

    if ((bool) last_time + (bool) last_time_off + (bool) interval > 1)
	return errh->error("END, END_AFTER, and INTERVAL are mutually exclusive");
    else if ((bool) last_time)
	_last_time = last_time;
    else if ((bool) last_time_off)
	_last_time = last_time_off, _last_time_relative = true;
    else if ((bool) interval)
	_last_time = interval, _last_time_interval = true;
    else
	_have_last_time = false;

    if (stop && end_h)
	return errh->error("END_CALL and STOP are mutually exclusive");
    else if (end_h)
	_end_h = new HandlerCall(end_h);
    else if (stop)
	_end_h = new HandlerCall(name() + ".stop");
    else if (_have_last_time)
	_end_h = new HandlerCall(name() + ".active false");

    // default linktype
    if (!encap)
	_base_linktype = FAKE_DLT_NONE;
    else if ((_base_linktype = fake_pcap_parse_dlt(encap)) < 0
	     || (_base_linktype != FAKE_DLT_SUNATM
		 && _base_linktype != FAKE_DLT_C_HDLC
		 && _base_linktype != FAKE_DLT_EN10MB
		 && _base_linktype != FAKE_DLT_ATM_RFC1483
		 && _base_linktype != FAKE_DLT_PPP
		 && _base_linktype != FAKE_DLT_PPP_HDLC
		 && _base_linktype != FAKE_DLT_RAW))
	return errh->error("bad encapsulation type");

    // set other variables
    _have_any_times = false;
    _timing = timing;
    _force_ip = force_ip;
    _linktype = FAKE_DLT_NONE;
    _active = active;
    return 0;
}

int
FromDAGDump::initialize(ErrorHandler *errh)
{
    if (_ff.initialize(errh) < 0)
	return -1;

    // if forcing IP packets, check we're not running TIMING
    if (_force_ip && _timing)
	return errh->error("FORCE_IP and TIMING options are incompatible");

    // check handler call
    if (_end_h && _end_h->initialize_write(this, errh) < 0)
	return -1;

    // try reading a packet
    if (read_packet(errh))
	_time_offset = Timestamp::now() - _packet->timestamp_anno();

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
FromDAGDump::stamp_to_time(uint64_t stamp, Timestamp &tv) const
{
    uint32_t sec = (uint32_t) (stamp >> 32);
    // based on a code description in an Endace document
    stamp = (stamp & 0xFFFFFFFFULL) * 1000000000;
    stamp += (stamp & 0x80000000ULL) << 1; // rounding
    uint32_t nsec = (uint32_t) (stamp >> 32);
    if (nsec >= 1000000000) {
	nsec -= 1000000000;
	sec += 1;
    }
    tv = Timestamp::make_nsec(sec, nsec);
}

void
FromDAGDump::prepare_times(const Timestamp &tv)
{
    if (_first_time_relative)
	_first_time += tv;
    if (_last_time_relative)
	_last_time += tv;
    else if (_last_time_interval)
	_last_time += _first_time;
    _have_any_times = true;
}

bool
FromDAGDump::read_packet(ErrorHandler *errh)
{
    const DAGCell *cell;
    static DAGCell static_cell;
    Timestamp tv;
    Packet *p;
    bool more = true;
    _packet = 0;

  retry:
    // quit if we sampled or force_ip failed, but we are no longer active
    if (!more)
	return false;

    // we may need to read bits of the file
    cell = reinterpret_cast<const DAGCell *>(_ff.get_aligned(DAGCell::HEADER_SIZE, &static_cell, errh));
    if (!cell)
	return false;

    // check times
  check_times:
    stamp_to_time(swapq(cell->timestamp), tv);
    if (!_have_any_times)
	prepare_times(tv);
    if (_have_first_time) {
	if (tv < _first_time)
	    goto retry;
	else
	    _have_first_time = false;
    }
    if (_have_last_time && tv >= _last_time) {
	_have_last_time = false;
	(void) _end_h->call_write(errh);
	if (!_active)
	    more = false;
	// retry _last_time in case someone changed it
	goto check_times;
    }

    // checking sampling probability
    if (_sampling_prob < (1 << SAMPLING_SHIFT)
	&& (click_random() & ((1<<SAMPLING_SHIFT)-1)) >= _sampling_prob)
	goto retry;

    // determine read length and wire length
    uint32_t wire_length = 0;
    if (cell->type == DAGCell::TYPE_LEGACY || _base_linktype >= 0) {
      use_base_linktype:
	_linktype = _base_linktype;
	switch (_base_linktype) {

	  cell:
	  case FAKE_DLT_ATM_RFC1483:
	  case FAKE_DLT_PPP:
	  case FAKE_DLT_PPP_HDLC:
	    p = _ff.get_packet(DAGCell::CELL_SIZE - DAGCell::HEADER_SIZE, tv.sec(), tv.subsec(), errh);
	    break;

	  case FAKE_DLT_C_HDLC:
	    wire_length = htons(*(reinterpret_cast<const uint16_t*>(cell) + 5));
	    goto cell;

	  case FAKE_DLT_NONE:
	    _linktype = FAKE_DLT_ATM_RFC1483;
	    goto cell;

	  case FAKE_DLT_SUNATM:
	    p = _ff.get_packet_from_data(reinterpret_cast<const uint8_t*>(cell) + 12, 4, DAGCell::CELL_SIZE - 12, tv.sec(), tv.subsec(), errh);
	    break;

	  case FAKE_DLT_EN10MB:
	    wire_length = htons(*(reinterpret_cast<const uint16_t*>(cell) + 4));
	    p = _ff.get_packet_from_data(reinterpret_cast<const uint8_t*>(cell) + 10, 6, DAGCell::CELL_SIZE - 10, tv.sec(), tv.subsec(), errh);
	    break;

	  default:
	    p = _ff.get_packet_from_data(reinterpret_cast<const uint8_t*>(cell) + 8, 8, DAGCell::CELL_SIZE - 8, tv.sec(), tv.subsec(), errh);
	    break;
	}

    } else {
	int read_length = htons(cell->rlen);
	wire_length = htons(cell->wlen);
	switch (cell->type) {
	  case DAGCell::TYPE_ATM:
	  case DAGCell::TYPE_AAL5:
	    _linktype = FAKE_DLT_SUNATM;
	    break;
	  case DAGCell::TYPE_ETH:
	    _ff.shift_pos(2);
	    read_length -= 2;
	    wire_length -= 4;	// XXX DAG 'wlen' includes CRC
	    _linktype = FAKE_DLT_EN10MB;
	    break;
	  case DAGCell::TYPE_HDLC_POS:
	    _linktype = FAKE_DLT_C_HDLC;
	    break;
	  default:		// indicates an old-format dump
	    if (_base_linktype == FAKE_DLT_NONE)
		_base_linktype = FAKE_DLT_ATM_RFC1483;
	    if (errh) {
		errh->warning("odd DAG cell type %d, assuming old-style ATM encapsulation", cell->type);
		errh->message("(To avoid this warning, specify an explicit ENCAP.)");
	    } else
		click_chatter("%p{element}: DAG cell with odd type %d, assuming old-style\n  ATM encapsulation for rest of dump.  Packets may have been read incorrectly!\n  (To avoid this warning, specify an explicit ENCAP.)", this, cell->type);
	    goto use_base_linktype;
	}
	if (read_length < DAGCell::HEADER_SIZE)
	    return false;
	p = _ff.get_packet(read_length - DAGCell::HEADER_SIZE, tv.sec(), tv.subsec(), errh);
    }

    // check packet
    if (!p)
	return false;
    if (wire_length)
	SET_EXTRA_LENGTH_ANNO(p, wire_length - p->length());

    if (_force_ip && !fake_pcap_force_ip(p, _linktype)) {
	checked_output_push(1, p);
	goto retry;
    }

    _packet = p;
    return more;
}

bool
FromDAGDump::run_task(Task *)
{
    if (!_active)
	return false;

    bool more;
    if (_packet || read_packet(0)) {
	if (_timing
	    && _packet->timestamp_anno() > Timestamp::now() - _time_offset) {
	    _task.fast_reschedule();
	    return false;
	}
	output(0).push(_packet);
	more = read_packet(0);
    } else
	more = false;

    if (more)
	_task.fast_reschedule();
    else if (_end_h)
	_end_h->call_write(ErrorHandler::default_handler());
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
	if (_timing
	    && _packet->timestamp_anno() > Timestamp::now() - _time_offset)
	    return 0;
	p = _packet;
	more = read_packet(0);
    } else {
	p = 0;
	more = false;
    }

    if (!more && _end_h)
	_end_h->call_write(ErrorHandler::default_handler());
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
	return cp_unparse_real2(fd->_sampling_prob, SAMPLING_SHIFT);
      case H_ENCAP:
	return String(fake_pcap_unparse_dlt(fd->_linktype));
      default:
	return "<error>";
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
	  if (BoolArg().parse(s, active)) {
	      fd->set_active(active);
	      return 0;
	  } else
	      return errh->error("type mismatch");
      }
      case H_STOP:
	fd->set_active(false);
	fd->router()->please_stop_driver();
	return 0;
      case H_EXTEND_INTERVAL: {
	  Timestamp tv;
	  if (cp_time(s, &tv)) {
	      fd->_last_time += tv;
	      if (fd->_end_h)
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
    add_read_handler("sampling_prob", read_handler, H_SAMPLING_PROB);
    add_data_handlers("active", Handler::f_read | Handler::f_checkbox, &_active);
    add_write_handler("active", write_handler, H_ACTIVE);
    add_read_handler("encap", read_handler, H_ENCAP);
    add_write_handler("stop", write_handler, H_STOP);
    add_write_handler("extend_interval", write_handler, H_EXTEND_INTERVAL);
    _ff.add_handlers(this);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel int64 FakePcap)
EXPORT_ELEMENT(FromDAGDump)
