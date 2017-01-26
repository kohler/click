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
#include <sys/mman.h>
#endif

#define	SWAPLONG(y) \
	((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))
#define	SWAPSHORT(y) \
	( (((y)&0xff)<<8) | ((u_short)((y)&0xff00)>>8) )

FromFlanDump::FromFlanDump()
    : _task(this)
{
    for (int i = 0; i < FF_LAST; i++)
	_ff[i] = 0;
}

FromFlanDump::~FromFlanDump()
{
}

int
FromFlanDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true;
    bool have_packets, packets, have_flows, flows;

    if (Args(conf, this, errh)
	.read_mp("FILENAME", FilenameArg(), _dirname)
	.read("STOP", stop)
	.read("ACTIVE", active)
	.read("PACKETS", have_packets, packets)
	.read("FLOWS", have_flows, flows)
	.complete() < 0)
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
FromFlanDump::FlanFile::FlanFile()
    : _fd(-1), _buffer(0), _offset(0), _len(0), _pipe(0), _my_buffer(true)
{
}

void
FromFlanDump::FlanFile::~FlanFile()
{
    if (_pipe)
	pclose(_pipe);
    else if (_fd >= 0)
	close(_fd);
    if (_my_buffer)
	delete[] _buffer;
}

int
FromFlanDump::FlanFile::open(const String &basename, const String &filename, int record_size, ErrorHandler *errh)
{
    assert(!_pipe && _fd < 0);
    _record_size = record_size;

    String path = basename;
    if (basename.back() != '/')
	path += "/";
    path += filename;

    // look for compressed versions
    if (access(path.c_str(), R_OK) >= 0) {
	_fd = open(path.c_str(), O_RDONLY);
	if (_fd < 0)
	    return errh->error("%s: %s", path.c_str(), strerror(errno));
    } else if (access((path + ".gz").c_str(), R_OK) >= 0) {
	char buf[3] = "\037\213";
	_pipe = open_uncompress_pipe(path + ".gz", (unsigned char *)buf, 2, errh);
	return (_fd = (_pipe ? fileno(_pipe) : -1));
    } else if (access((path + ".bz2").c_str(), R_OK) >= 0) {
	char buf[3] = "BZh";
	_pipe = open_uncompress_pipe(path + ".bz2", (unsigned char *)buf, 3, errh);
	return (_fd = (_pipe ? fileno(_pipe) : -1));
    } else
	return errh->error("%s: no such file", path.c_str());
}

int
FromFlanDump::FlanFile::read_more(off_t start_off)
{
    if (_offset + _len != start_off)
	if (lseek(_fd, start_off, SEEK_SET) == (off_t)-1) {
	    _offset = _len = 0;
	    // XXX
	    return -1;
	}

    // read data
    if (!_buffer && !(_buffer = new uint8_t[BUFFER_SIZE]))
	return -ENOMEM;

    _offset += _len;
    _len = 0;

    while (_len < BUFFER_SIZE) {
	ssize_t got = read(_fd, _buffer + _len, BUFFER_SIZE - _len);
	if (got > 0)
	    _len += got;
	else if (got == 0)	// premature end of file
	    return 0;
	else if (got < 0 && errno != EINTR && errno != EAGAIN)
	    return -1; //XXX error_helper(errh, strerror(errno));
    }

    return 0;
}

int
FromFlanDump::error_helper(ErrorHandler *errh, const char *x)
{
    if (errh)
	errh->error("%s: %s", _dirname.c_str(), x);
    else
	click_chatter("%s: %s", declaration().c_str(), x);
    return -1;
}

int
FromFlanDump::initialize(ErrorHandler *errh)
{

    if (_filename == "-") {
	_fd = STDIN_FILENO;
	_filename = "<stdin>";
    } else
	_fd = open(_filename.c_str(), O_RDONLY);
    if (_fd < 0)
	return errh->error("%s: %s", _filename.c_str(), strerror(errno));

  retry_file:
#ifdef ALLOW_MMAP
    _mmap_unit = 0;
#endif
    _file_offset = 0;
    int result = read_buffer(errh);
    if (result < 0)
	return -1;
    else if (result == 0)
	return errh->error("%s: empty file", _filename.c_str());

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
	    return errh->error("%s: unknown linktype %d; can't force IP packets", _filename.c_str(), _linktype);
	if (_timing)
	    return errh->error("FORCE_IP and TIMING options are incompatible");
    }

    // check handler call
    if (_last_time_h && _last_time_h->initialize_write(this, errh) < 0)
	return -1;

    // try reading a packet
    _pos = 0;
    if (read_packet(errh))
	_time_offset = Timestamp::now() - _packet->timestamp_anno();

    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return 0;
}

void
FromFlanDump::cleanup(CleanupStage)
{
    for (int i = 0; i < FF_LAST; i++)
	delete _ff[i];
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


Packet *
FromFlanDump::read_flow_packet()
{
    if (_record >= _last_record) {
	assert(_record == _last_record);
	_last_record = 0;
	for (int i = FF_FIRST_FLOW; i < FF_LAST_FLOW; i++)
	    if (_ff[i]) {
		_ff[i]->read_more(_record);
		if (_last_record == 0 || _ff[i]->last_record() < _last_record)
		    _last_record = _ff[i]->last_record();
	    }
	if (_record >= _last_record)
	    return 0;
    }

    WritablePacket *q = Packet::make(0, sizeof(click_ip) + sizeof(click_tcp));
    if (!q) {
	error_helper("out of memory!");
	return 0;
    }

    q->set_network_header(q->data(), sizeof(click_ip));
    click_ip *iph = q->ip_header();
    iph->ip_v = 4;
    iph->ip_hl = (sizeof(click_ip) >> 2);
    if (_ff[FF_SADDR])
	iph->ip_src.s_addr = _ff[FF_SADDR]->read_uint32(_record);
    if (_ff[FF_DADDR])
	iph->ip_dst.s_addr = _ff[FF_DADDR]->read_uint32(_record);

    click_tcp *tcph = q->tcp_header();
    if (_ff[FF_SPORT])
	tcph->th_sport = _ff[FF_SPORT]->read_uint16(_record);
    if (_ff[FF_DPORT])
	tcph->th_dport = _ff[FF_DPORT]->read_uint16(_record);

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
	// retry _last_time in case someone changed it
	goto check_times;
    }

    // checking sampling probability
    if (_sampling_prob < (1 << SAMPLING_SHIFT)
	&& (click_random() & ((1<<SAMPLING_SHIFT)-1)) >= _sampling_prob)
	goto retry;

    // create packet
    if (cell != &static_cell) {
	p = _data_packet->clone();
	if (!p) {
	    error_helper(errh, "out of memory!");
	    return false;
	}
	p->shrink_packet(_pos - sizeof(DAGCell) + DAGCell::PAYLOAD_OFFSET, sizeof(DAGCell) - DAGCell::PAYLOAD_OFFSET);
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

bool
FromFlanDump::run_task(Task *)
{
    if (!_active)
	return;

    bool more;
    if (_packet || read_packet(0)) {
	if (_timing
	    && _packet->timestamp_anno() + _time_offset >= Timestamp::now()) {
	    _task.fast_reschedule();
	    return;
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
	if (_timing
	    && _packet->timestamp_anno() + _time_offset >= Timestamp::now())
	    return 0;
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
    switch ((intptr_t)thunk) {
      case ACTIVE_THUNK:
	return BoolArg::unparse(fd->_active);
      case FILESIZE_THUNK: {
	  struct stat s;
	  if (fd->_fd >= 0 && fstat(fd->_fd, &s) >= 0 && S_ISREG(s.st_mode))
	      return String(s.st_size);
	  else
	      return "-";
      }
      case FILEPOS_THUNK:
	return String(fd->_file_offset + fd->_pos);
      default:
	return "<error>";
    }
}

int
FromFlanDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromFlanDump *fd = static_cast<FromFlanDump *>(e);
    String s = cp_uncomment(s_in);
    switch ((intptr_t)thunk) {
      case ACTIVE_THUNK: {
	  bool active;
	  if (BoolArg().parse(s, active)) {
	      fd->set_active(active);
	      return 0;
	  } else
	      return errh->error("type mismatch");
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
    add_read_handler("active", read_handler, ACTIVE_THUNK, Handler::f_checkbox);
    add_write_handler("active", write_handler, ACTIVE_THUNK);
    add_write_handler("stop", write_handler, STOP_THUNK, Handler::f_button);
    add_read_handler("filesize", read_handler, FILESIZE_THUNK);
    add_read_handler("filepos", read_handler, FILEPOS_THUNK);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel int64 false)
EXPORT_ELEMENT(FromFlanDump)
