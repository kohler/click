// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdagdump.{cc,hh} -- element reads packets from tcpdump file
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
#include <click/packet_anno.hh>
#include <click/rfc1483.h>
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

FromDAGDump::FromDAGDump()
    : Element(0, 1), _fd(-1), _buffer(0), _data_packet(0), _packet(0),
      _last_time_h(0), _task(this), _pipe(0)
{
    MOD_INC_USE_COUNT;
    static_assert(sizeof(DAGCell) == 64);
    static_assert(sizeof(((DAGCell *) 0)->payload) == sizeof(DAGCell) - DAGCell::PAYLOAD_OFFSET);
}

FromDAGDump::~FromDAGDump()
{
    MOD_DEC_USE_COUNT;
    delete _last_time_h;
    uninitialize();
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
#ifdef __linux__
    bool mmap = false;
#else
    bool mmap = true;
#endif
    struct timeval first_time, first_time_off, last_time, last_time_off, interval;
    timerclear(&first_time);
    timerclear(&first_time_off);
    timerclear(&last_time);
    timerclear(&last_time_off);
    timerclear(&interval);
    _sampling_prob = (1 << SAMPLING_SHIFT);
    
    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump file name", &_filename,
		    cpOptional,
		    cpBool, "use original packet timing?", &timing,
		    cpKeywords,
		    "TIMING", cpBool, "use original packet timing?", &timing,
		    "STOP", cpBool, "stop driver when done?", &stop,
		    "ACTIVE", cpBool, "start active?", &active,
		    "MMAP", cpBool, "access file with mmap()?", &mmap,
		    "SAMPLE", cpUnsignedReal2, "sampling probability", SAMPLING_SHIFT, &_sampling_prob,
		    "FORCE_IP", cpBool, "emit IP packets only?", &force_ip,
		    "START", cpTimeval, "starting time", &first_time,
		    "START_AFTER", cpTimeval, "starting time offset", &first_time_off,
		    "END", cpTimeval, "ending time", &last_time,
		    "END_AFTER", cpTimeval, "ending time offset", &last_time_off,
		    "INTERVAL", cpTimeval, "time interval", &interval,
		    "END_CALL", cpWriteHandlerCall, "write handler for ending time", &_last_time_h,
		    0) < 0)
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
	return errh->error("`START' and `START_AFTER' are mutually exclusive");
    else if (timerisset(&first_time))
	_first_time = first_time;
    else if (timerisset(&first_time_off))
	_first_time = first_time_off, _first_time_relative = true;
    else {
	timerclear(&_first_time);
	_have_first_time = false, _first_time_relative = true;
    }
    
    if ((timerisset(&last_time) != 0) + (timerisset(&last_time_off) != 0) + (timerisset(&interval) != 0) > 1)
	return errh->error("`END', `END_AFTER', and `INTERVAL' are mutually exclusive");
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
#ifdef ALLOW_MMAP
    _mmap = mmap;
#else
    if (mmap)
	errh->warning("`MMAP' is not supported on this platform");
#endif
    _active = active;
    return 0;
}

int
FromDAGDump::error_helper(ErrorHandler *errh, const char *x)
{
    if (errh)
	errh->error("%s: %s", _filename.cc(), x);
    else
	click_chatter("%s: %s", declaration().cc(), x);
    return -1;
}

#ifdef ALLOW_MMAP
static void
munmap_destructor(unsigned char *data, size_t amount)
{
    if (munmap((caddr_t)data, amount) < 0)
	click_chatter("FromDAGDump: munmap: %s", strerror(errno));
}

int
FromDAGDump::read_buffer_mmap(ErrorHandler *errh)
{
    if (_mmap_unit == 0) {  
	size_t page_size = getpagesize();
	_mmap_unit = (WANT_MMAP_UNIT / page_size) * page_size;
	_mmap_off = 0;
	// don't report most errors on the first time through
	errh = ErrorHandler::silent_handler();
    }

    // get length of file
    struct stat statbuf;
    if (fstat(_fd, &statbuf) < 0)
	return error_helper(errh, String("stat: ") + strerror(errno));

    // check for end of file
    // But return -1 if we have not mmaped before: it might be a pipe, not
    // true EOF.
    if (_mmap_off >= statbuf.st_size)
	return (_mmap_off == 0 ? -1 : 0);

    // actually mmap
    _len = _mmap_unit;
    if ((off_t)(_mmap_off + _len) > statbuf.st_size)
	_len = statbuf.st_size - _mmap_off;
    
    void *mmap_data = mmap(0, _len, PROT_READ, MAP_SHARED, _fd, _mmap_off);

    if (mmap_data == MAP_FAILED)
	return error_helper(errh, String("mmap: ") + strerror(errno));

    _data_packet = Packet::make((unsigned char *)mmap_data, _len, munmap_destructor);
    _buffer = _data_packet->data();
    _file_offset = _mmap_off;
    _mmap_off += _len;

#ifdef HAVE_MADVISE
    // don't care about errors
    (void) madvise((caddr_t)mmap_data, _len, MADV_SEQUENTIAL);
#endif
    
    return 1;
}
#endif

int
FromDAGDump::read_buffer(ErrorHandler *errh)
{
    if (_data_packet)
	_data_packet->kill();
    _data_packet = 0;

    _file_offset += _len;
    _pos -= _len;		// adjust _pos by _len: it might validly point
				// beyond _len
    _len = 0;

#ifdef ALLOW_MMAP
    if (_mmap) {
	int result = read_buffer_mmap(errh);
	if (result >= 0)
	    return result;
	// else, try a regular read
	_mmap = false;
	(void) lseek(_fd, _mmap_off, SEEK_SET);
	_len = 0;
    }
#endif
    
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
FromDAGDump::read_into(void *vdata, uint32_t dlen, ErrorHandler *errh)
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
FromDAGDump::initialize(ErrorHandler *errh)
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
    if (result < 0) {
	uninitialize();
	return -1;
    } else if (result == 0) {
	uninitialize();
	return errh->error("%s: empty file", _filename.cc());
    }

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
FromDAGDump::uninitialize()
{
    if (_pipe)
	pclose(_pipe);
    else if (_fd >= 0 && _fd != STDIN_FILENO)
	close(_fd);
    if (_packet)
	_packet->kill();
    if (_data_packet)
	_data_packet->kill();
    _fd = -1;
    _packet = _data_packet = 0;
    _pipe = 0;
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
FromDAGDump::run_scheduled()
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
    SAMPLING_PROB_THUNK, ACTIVE_THUNK, ENCAP_THUNK, STOP_THUNK,
    FILESIZE_THUNK, FILEPOS_THUNK, EXTEND_INTERVAL_THUNK
};

String
FromDAGDump::read_handler(Element *e, void *thunk)
{
    FromDAGDump *fd = static_cast<FromDAGDump *>(e);
    switch ((int)thunk) {
      case SAMPLING_PROB_THUNK:
	return cp_unparse_real2(fd->_sampling_prob, SAMPLING_SHIFT) + "\n";
      case ACTIVE_THUNK:
	return cp_unparse_bool(fd->_active) + "\n";
      case ENCAP_THUNK:
	return String(fake_pcap_unparse_dlt(fd->_linktype)) + "\n";
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
FromDAGDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromDAGDump *fd = static_cast<FromDAGDump *>(e);
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
      case EXTEND_INTERVAL_THUNK: {
	  struct timeval tv;
	  if (cp_timeval(s, &tv)) {
	      timeradd(&fd->_last_time, &tv, &fd->_last_time);
	      if (fd->_last_time_h)
		  fd->_have_last_time = true, fd->set_active(true);
	      return 0;
	  } else
	      return errh->error("`extend_interval' takes a time interval");
      }
      default:
	return -EINVAL;
    }
}

void
FromDAGDump::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, (void *)SAMPLING_PROB_THUNK);
    add_read_handler("active", read_handler, (void *)ACTIVE_THUNK);
    add_write_handler("active", write_handler, (void *)ACTIVE_THUNK);
    add_read_handler("encap", read_handler, (void *)ENCAP_THUNK);
    add_write_handler("stop", write_handler, (void *)STOP_THUNK);
    add_read_handler("filesize", read_handler, (void *)FILESIZE_THUNK);
    add_read_handler("filepos", read_handler, (void *)FILEPOS_THUNK);
    add_write_handler("extend_interval", write_handler, (void *)EXTEND_INTERVAL_THUNK);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel int64 FakePcap)
EXPORT_ELEMENT(FromDAGDump)
