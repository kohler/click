// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromnlanrdump.{cc,hh} -- element reads packets from NLANR format file
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
#include "fromnlanrdump.hh"
#include <click/confparse.hh>
#include <click/packet_anno.hh>
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

FromNLANRDump::FromNLANRDump()
    : _packet(0), _end_h(0), _task(this)
{
}

FromNLANRDump::~FromNLANRDump()
{
    delete _end_h;
}

int
FromNLANRDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool timing = false, stop = false, active = true;
    String format = "guess";
    Timestamp first_time, first_time_off, last_time, last_time_off, interval;
    _sampling_prob = (1 << SAMPLING_SHIFT);
    _packet_filepos = 0;
    bool force_ip = true;

    if (_ff.configure_keywords(conf, this, errh) < 0)
	return -1;
    if (cp_va_kparse(conf, this, errh,
		     "FILENAME", cpkP+cpkM, cpFilename, &_ff.filename(),
		     "FORMAT", 0, cpWord, &format,
		     "STOP", 0, cpBool, &stop,
		     "ACTIVE", 0, cpBool, &active,
		     "FORCE_IP", 0, cpBool, &force_ip,
		     "START", 0, cpTimestamp, &first_time,
		     "START_AFTER", 0, cpTimestamp, &first_time_off,
		     "END", 0, cpTimestamp, &last_time,
		     "END_AFTER", 0, cpTimestamp, &last_time_off,
		     "INTERVAL", 0, cpTimestamp, &interval,
		     "END_CALL", 0, cpHandlerCallPtrWrite, &_end_h,
		     "FILEPOS", 0, cpFileOffset, &_packet_filepos,
		     "SAMPLE", 0, cpUnsignedReal2, SAMPLING_SHIFT, &_sampling_prob,
		     "TIMING", 0, cpBool, &timing,
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
    
    if ((bool) first_time + (bool) first_time_off > 1)
	return errh->error("'START' and 'START_AFTER' are mutually exclusive");
    else if (first_time)
	_first_time = first_time;
    else if (first_time_off)
	_first_time = first_time_off, _first_time_relative = true;
    else
	_have_first_time = false, _first_time_relative = true;
    
    if ((bool) last_time + (bool) last_time_off + (bool) interval > 1)
	return errh->error("'END', 'END_AFTER', and 'INTERVAL' are mutually exclusive");
    else if (last_time)
	_last_time = last_time;
    else if (last_time_off)
	_last_time = last_time_off, _last_time_relative = true;
    else if (interval)
	_last_time = interval, _last_time_interval = true;
    else
	_have_last_time = false;

    if (stop && _end_h)
	return errh->error("'END_CALL' and 'STOP' are mutually exclusive");
    else if (stop)
	_end_h = new HandlerCall(name() + ".stop");
    else if (_have_last_time && !_end_h)
	_end_h = new HandlerCall(name() + ".active false");

    // format
    format = format.lower();
    if (format == "fr")
	_format = C_FR;
    else if (format == "fr+")
	_format = C_FRPLUS;
    else if (format == "tsh")
	_format = C_TSH;
    else if (format == "guess") {
	format = _ff.filename().lower();
	if (format.substring(-3).lower() == ".gz")
	    format = format.substring(0, format.length() - 3);
	else if (format.substring(-2).lower() == ".z")
	    format = format.substring(0, format.length() - 2);
	else if (format.substring(-4).lower() == ".bz2")
	    format = format.substring(0, format.length() - 4);
	if (format.substring(-3).lower() == ".fr")
	    _format = C_FR;
	else if (format.substring(-3).lower() == ".fr+")
	    _format = C_FRPLUS;
	else if (format.substring(-4).lower() == ".tsh")
	    _format = C_TSH;
	else {
	    errh->warning("cannot guess file format from filename, assuming 'tsh'");
	    _format = C_TSH;
	}
    } else
	return errh->error("bad FORMAT");
    switch (_format) {
      case C_FR: _cell_size = FRCell::SIZE; break;
      case C_FRPLUS: _cell_size = FRPlusCell::SIZE; break;
      case C_TSH: _cell_size = TSHCell::SIZE; break;
    }
    
    // set other variables
    _have_any_times = false;
    _timing = timing;
    _active = active;
    return 0;
}

int
FromNLANRDump::initialize(ErrorHandler *errh)
{
    // check handler call, initialize Task
    if (_end_h && _end_h->initialize_write(this, errh) < 0)
	return -1;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);

    // open file
    if (_ff.initialize(errh) < 0)
	return -1;
    
    // try reading a packet
    if (read_packet(errh))
	_time_offset = Timestamp::now() - _packet->timestamp_anno();

    // maybe skip ahead in the file
    if (_packet_filepos != 0) {
	int result = _ff.seek(_packet_filepos, errh);
	_packet_filepos = 0;
	return result;
    } else
	return 0;
}

void
FromNLANRDump::cleanup(CleanupStage)
{
    _ff.cleanup();
    if (_packet)
	_packet->kill();
    _packet = 0;
}

void
FromNLANRDump::set_active(bool active)
{
    if (_active != active) {
	_active = active;
	if (active && output_is_push(0) && !_task.scheduled())
	    _task.reschedule();
    }
}

void
FromNLANRDump::prepare_times(const Timestamp &tv)
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
FromNLANRDump::read_packet(ErrorHandler *errh)
{
    const TSHCell *cell;
    static TSHCell static_cell;
    Timestamp tv;
    Packet *p;
    bool more = true;
    _packet = 0;

  retry:
    // quit if we sampled or force_ip failed, but we are no longer active
    if (!more)
	return false;
    
    // record file position
    _packet_filepos = _ff.file_pos();

    // read the cell
    cell = reinterpret_cast<const TSHCell *>(_ff.get_aligned(_cell_size, &static_cell, errh));
    if (!cell)
	return false;

    // check times
  check_times:
    uint32_t usec = ntohl(cell->timestamp_usec);
    if (_format == C_TSH)
	tv = Timestamp(ntohl(cell->timestamp_sec), usec & 0xFFFFFF);
    else if (_format == C_FRPLUS || _format == C_FR)
	tv = Timestamp(ntohl(cell->timestamp_sec), usec);
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
	&& (uint32_t)(random() & ((1<<SAMPLING_SHIFT)-1)) >= _sampling_prob)
	goto retry;
    
    // create packet
    if (_format != C_FR)
	p = _ff.get_packet_from_data(&cell->iph, _cell_size - 8, _cell_size - 8, tv.sec(), tv.subsec(), errh);
    else
	p = 0;
    if (!p)
	return false;

    fake_pcap_force_ip(p, FAKE_DLT_RAW);
    if (p->network_header_length() != sizeof(click_ip)) { // there were ip options
	checked_output_push(1, p);
	goto retry;
    }

    if (_format == C_TSH)	// set link annotation
	SET_PAINT_ANNO(p, usec >> 24);
    _packet = p;
    return more;
}

bool
FromNLANRDump::run_task(Task *)
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
FromNLANRDump::pull(int)
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
    H_SAMPLING_PROB, H_ACTIVE, H_ENCAP, H_STOP, H_PACKET_FILEPOS,
    H_EXTEND_INTERVAL
};

String
FromNLANRDump::read_handler(Element *e, void *thunk)
{
    FromNLANRDump *fd = static_cast<FromNLANRDump *>(e);
    switch ((intptr_t)thunk) {
      case H_SAMPLING_PROB:
	return cp_unparse_real2(fd->_sampling_prob, SAMPLING_SHIFT);
      case H_ACTIVE:
	return cp_unparse_bool(fd->_active);
      case H_ENCAP:
	return "IP";
      case H_PACKET_FILEPOS:
	return String(fd->_packet_filepos);
      default:
	return "<error>";
    }
}

int
FromNLANRDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromNLANRDump *fd = static_cast<FromNLANRDump *>(e);
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
FromNLANRDump::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, (void *)H_SAMPLING_PROB);
    add_read_handler("active", read_handler, (void *)H_ACTIVE, Handler::CHECKBOX);
    add_write_handler("active", write_handler, (void *)H_ACTIVE);
    add_read_handler("encap", read_handler, (void *)H_ENCAP);
    add_write_handler("stop", write_handler, (void *)H_STOP, Handler::BUTTON);
    add_read_handler("packet_filepos", read_handler, (void *)H_PACKET_FILEPOS);
    add_write_handler("extend_interval", write_handler, (void *)H_EXTEND_INTERVAL);
    _ff.add_handlers(this);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel int64 FakePcap FromFile)
EXPORT_ELEMENT(FromNLANRDump)
