// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromflandump.{cc,hh} -- element reads packets from Chuck Blake's Flan file
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
#include "fromflandump.hh"
#include <click/confparse.hh>
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
#include <sys/mman.h>
#endif

#define	SWAPLONG(y) \
	((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))
#define	SWAPSHORT(y) \
	( (((y)&0xff)<<8) | ((u_short)((y)&0xff00)>>8) )

FromFlanDump::FromFlanDump()
    : Element(0, 1),
      _flid(0), _time(0), _size(0), _flags(0),
      _saddr(0), _sport(0), _daddr(0), _dport(0),
      _ct_pkt(0), _ct_bytes(0),
      _task(this), _pipe(0)
{
    MOD_INC_USE_COUNT;
}

FromFlanDump::~FromFlanDump()
{
    MOD_DEC_USE_COUNT;
}

int
FromFlanDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true;
    bool have_packets = false, packets, have_flows = false, flows;
    
    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump directory name", &_filename,
		    cpKeywords,
		    "STOP", cpBool, "stop driver when done?", &stop,
		    "ACTIVE", cpBool, "start active?", &active,
		    cpConfirmKeywords,
		    "PACKETS", cpBool, "output packets?", &have_packets, &packets,
		    "FLOWS", cpBool, "output flows?", &have_flows, &flows,
		    0) < 0)
	return -1;

    // check packets vs. flows
    if (have_packets && have_flows && packets == flows)
	return errh->error("can't specify both PACKETS and FLOWS");
    else if (have_packets)
	flows = !packets;
    else if (!have_packets && !have_flows)
	flows = true;
    _flows = flows;

    // set other variables
    _stop = stop;
    _active = active;
    return 0;
}

void
FromFlanDump::FlanFile::~FlanFile()
{
    if (fd >= 0)
	close(fd);
    delete[] buffer;
}

int
FromFlanDump::error_helper(ErrorHandler *errh, const char *x)
{
    if (errh)
	errh->error("%s: %s", _filename.cc(), x);
    else
	click_chatter("%s: %s", declaration().cc(), x);
    return -1;
}

int
FromFlanDump::read_buffer(ErrorHandler *errh)
{
    if (_data_packet)
	_data_packet->kill();
    _data_packet = 0;

    _file_offset += _len;
    _pos -= _len;		// adjust _pos by _len: it might validly point
				// beyond _len
    _len = 0;
    
    _data_packet = Packet::make(0, 0, BUFFER_SIZE, 0);
    if (!_data_packet)
	return errh->error("out of memory!");
    _buffer = _data_packet->data();
    unsigned char *data = _data_packet->data();
    assert(_data_packet->headroom() == 0);
    
    while (_len < BUFFER_SIZE) {
	ssize_t got = read(_fd, data + _len, BUFFER_SIZE - _len);
	if (got > 0)
	    _len += got;
	else if (got == 0)	// premature end of file
	    return _len;
	else if (got < 0 && errno != EINTR && errno != EAGAIN)
	    return error_helper(errh, strerror(errno));
    }
    
    return _len;
}

int
FromFlanDump::read_into(void *vdata, uint32_t dlen, ErrorHandler *errh)
{
    unsigned char *data = reinterpret_cast<unsigned char *>(vdata);
    uint32_t dpos = 0;

    while (dpos < dlen) {
	if (_pos < _len) {
	    uint32_t howmuch = dlen - dpos;
	    if (howmuch > _len - _pos)
		howmuch = _len - _pos;
	    memcpy(data + dpos, _buffer + _pos, howmuch);
	    dpos += howmuch;
	    _pos += howmuch;
	}
	if (dpos < dlen && read_buffer(errh) <= 0)
	    return dpos;
    }

    return dlen;
}

int
FromFlanDump::initialize(ErrorHandler *errh)
{
    if (_filename == "-") {
	_fd = STDIN_FILENO;
	_filename = "<stdin>";
    } else
	_fd = open(_filename.cc(), O_RDONLY);
    if (_fd < 0)
	return errh->error("%s: %s", _filename.cc(), strerror(errno));

  retry_file:
#ifdef ALLOW_MMAP
    _mmap_unit = 0;
#endif
    _file_offset = 0;
    int result = read_buffer(errh);
    if (result < 0)
	return -1;
    else if (result == 0)
	return errh->error("%s: empty file", _filename.cc());

    // check for a gziped or bzip2d dump
    if (_fd == STDIN_FILENO || _pipe)
	/* cannot handle gzip or bzip2 */;
    else if (compressed_data(_buffer, _len)
	     && (_len < DAGCell::PAYLOAD_OFFSET + RFC1483_SNAP_EXPECTED_LEN
		 || memcmp(_buffer + DAGCell::PAYLOAD_OFFSET, RFC1483_SNAP_EXPECTED, RFC1483_SNAP_EXPECTED_LEN) != 0)) {
	close(_fd);
	_fd = -1;
	if (!(_pipe = open_uncompress_pipe(_filename, _buffer, _len, errh)))
	    return -1;
	_fd = fileno(_pipe);
	goto retry_file;
    }
    
    // if forcing IP packets, check datalink type to ensure we understand it
    if (_force_ip) {
	if (!fake_pcap_dlt_force_ipable(_linktype))
	    return errh->error("%s: unknown linktype %d; can't force IP packets", _filename.cc(), _linktype);
	if (_timing)
	    return errh->error("FORCE_IP and TIMING options are incompatible");
    }

    // check handler call
    if (_last_time_h && _last_time_h->initialize_write(this, errh) < 0)
	return -1;
    
    // try reading a packet
    _pos = 0;
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
FromFlanDump::cleanup(CleanupStage)
{
    if (_pipe)
	pclose(_pipe);
    else if (_fd >= 0 && _fd != STDIN_FILENO)
	close(_fd);
    _fd = -1;
    _pipe = 0;
    if (_packet)
	_packet->kill();
    if (_data_packet)
	_data_packet->kill();
    _packet = _data_packet = 0;
}

void
FromFlanDump::set_active(bool active)
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
FromFlanDump::stamp_to_timeval(uint64_t stamp, struct timeval &tv) const
{
    tv.tv_sec = (uint32_t) (stamp >> 32);
    tv.tv_usec = (uint32_t) ((stamp * 1000000) >> 32);
}

void
FromFlanDump::prepare_times(struct timeval &tv)
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
FromFlanDump::read_packet(ErrorHandler *errh)
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
    if (_pos + sizeof(DAGCell) <= _len) {
	cell = reinterpret_cast<const DAGCell *>(_buffer + _pos);
	_pos += sizeof(DAGCell);
    } else {
	cell = &static_cell;
	if (read_into(&static_cell, sizeof(DAGCell), errh) < (int)sizeof(DAGCell))
	    return false;
    }

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
	(void) _last_time_h->call_write(this, errh);
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
    if (cell != &static_cell) {
	p = _data_packet->clone();
	if (!p) {
	    error_helper(errh, "out of memory!");
	    return false;
	}
	p->change_headroom_and_length(_pos - sizeof(DAGCell) + DAGCell::PAYLOAD_OFFSET, sizeof(DAGCell) - DAGCell::PAYLOAD_OFFSET);
	p->set_timestamp_anno(tv);
	
    } else {
	WritablePacket *wp = Packet::make(0, 0, sizeof(DAGCell) - DAGCell::PAYLOAD_OFFSET, 0);
	if (!wp) {
	    error_helper(errh, "out of memory!");
	    return false;
	}
	memcpy(wp->data(), &cell->payload, sizeof(cell->payload));
	wp->set_timestamp_anno(tv);
	
	p = wp;
    }

    if (_force_ip && !fake_pcap_force_ip(p, _linktype)) {
	checked_output_push(1, p);
	goto retry;
    }

    _packet = p;
    return more;
}

void
FromFlanDump::run_scheduled()
{
    if (!_active)
	return;

    bool more;
    if (_packet || read_packet(0)) {
	if (_timing) {
	    struct timeval now;
	    click_gettimeofday(&now);
	    timersub(&now, &_time_offset, &now);
	    if (timercmp(&_packet->timestamp_anno(), &now, >)) {
		_task.fast_reschedule();
		return;
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
}

Packet *
FromFlanDump::pull(int)
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
    ACTIVE_THUNK, STOP_THUNK, FILESIZE_THUNK, FILEPOS_THUNK
};

String
FromFlanDump::read_handler(Element *e, void *thunk)
{
    FromFlanDump *fd = static_cast<FromFlanDump *>(e);
    switch ((int)thunk) {
      case ACTIVE_THUNK:
	return cp_unparse_bool(fd->_active) + "\n";
      case FILESIZE_THUNK: {
	  struct stat s;
	  if (fd->_fd >= 0 && fstat(fd->_fd, &s) >= 0 && S_ISREG(s.st_mode))
	      return String(s.st_size) + "\n";
	  else
	      return "-\n";
      }
      case FILEPOS_THUNK:
	return String(fd->_file_offset + fd->_pos) + "\n";
      default:
	return "<error>\n";
    }
}

int
FromFlanDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromFlanDump *fd = static_cast<FromFlanDump *>(e);
    String s = cp_uncomment(s_in);
    switch ((int)thunk) {
      case ACTIVE_THUNK: {
	  bool active;
	  if (cp_bool(s, &active)) {
	      fd->set_active(active);
	      return 0;
	  } else
	      return errh->error("`active' should be Boolean");
      }
      case STOP_THUNK:
	fd->set_active(false);
	fd->router()->please_stop_driver();
	return 0;
      default:
	return -EINVAL;
    }
}

void
FromFlanDump::add_handlers()
{
    add_read_handler("active", read_handler, (void *)ACTIVE_THUNK);
    add_write_handler("active", write_handler, (void *)ACTIVE_THUNK);
    add_write_handler("stop", write_handler, (void *)STOP_THUNK);
    add_read_handler("filesize", read_handler, (void *)FILESIZE_THUNK);
    add_read_handler("filepos", read_handler, (void *)FILEPOS_THUNK);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel int64 false)
EXPORT_ELEMENT(FromFlanDump)
