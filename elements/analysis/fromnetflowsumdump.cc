// -*- c-basic-offset: 4 -*-
/*
 * fromnetflowsumdump.{cc,hh} -- element reads packets from NetFlow summary
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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

#include "fromnetflowsumdump.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <click/userutils.hh>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
CLICK_DECLS

FromNetFlowSummaryDump::FromNetFlowSummaryDump()
    : Element(0, 1), _fd(-1), _pos(0), _len(0), _work_packet(0),
      _task(this), _pipe(0)
{
    MOD_INC_USE_COUNT;
}

FromNetFlowSummaryDump::~FromNetFlowSummaryDump()
{
    MOD_DEC_USE_COUNT;
}

void *
FromNetFlowSummaryDump::cast(const char *n)
{
    if (strcmp(n, "Notifier") == 0 && !output_is_push(0)) {
	_notifier.initialize(router());
	return &_notifier;
    } else
	return Element::cast(n);
}

int
FromNetFlowSummaryDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true, zero = false, multipacket = false;
    
    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump file name", &_filename,
		    cpKeywords,
		    "STOP", cpBool, "stop driver when done?", &stop,
		    "ACTIVE", cpBool, "start active?", &active,
		    "ZERO", cpBool, "zero packet data?", &zero,
		    "MULTIPACKET", cpBool, "generate multiple packets per flow?", &multipacket,
		    0) < 0)
	return -1;

    _stop = stop;
    _active = active;
    _zero = zero;
    _multipacket = multipacket;
    return 0;
}

int
FromNetFlowSummaryDump::error_helper(ErrorHandler *errh, const char *x)
{
    if (errh)
	errh->error("%s: %s", _filename.cc(), x);
    else
	click_chatter("%s: %s", id().cc(), x);
    return -1;
}

int
FromNetFlowSummaryDump::read_buffer(ErrorHandler *errh)
{
    if (_pos == 0 && _len == _buffer.length())
	_buffer.append_garbage(BUFFER_SIZE);

    unsigned char *data = (unsigned char *)_buffer.mutable_data();
    int buffer_len = _buffer.length();

    if (_len == buffer_len) {
	memmove(data, data + _pos, _len - _pos);
	_len -= _pos;
	_file_offset += _pos;
	_pos = 0;
    }
    int initial_len = _len;
    
    while (_len < buffer_len) {
	ssize_t got = read(_fd, data + _len, buffer_len - _len);
	if (got > 0)
	    _len += got;
	else if (got == 0)	// premature end of file
	    return _len - initial_len;
	else if (got < 0 && errno != EINTR && errno != EAGAIN)
	    return error_helper(errh, strerror(errno));
    }
    
    return _len - initial_len;
}

int
FromNetFlowSummaryDump::read_line(String &result, ErrorHandler *errh)
{
    int epos = _pos;

    while (1) {
	bool done = false;
	
	if (epos >= _len) {
	    int delta = epos - _pos;
	    int errcode = read_buffer(errh);
	    if (errcode < 0 || (errcode == 0 && delta == 0))	// error
		return errcode;
	    else if (errcode == 0)
		done = true;
	    epos = _pos + delta;
	}

	const char *d = _buffer.data();
	while (epos < _len && d[epos] != '\n' && d[epos] != '\r')
	    epos++;

	if (epos < _len || done) {
	    result = _buffer.substring(_pos, epos - _pos);
	    if (epos < _len && d[epos] == '\r')
		epos++;
	    if (epos < _len && d[epos] == '\n')
		epos++;
	    _pos = epos;
	    return 1;
	}
    }
}

int
FromNetFlowSummaryDump::initialize(ErrorHandler *errh)
{
    if (!output_is_push(0))
	_notifier.initialize(router());
    
    _pipe = 0;
    if (_filename == "-") {
	_fd = STDIN_FILENO;
	_filename = "<stdin>";
    } else
	_fd = open(_filename.cc(), O_RDONLY);

  retry_file:
    if (_fd < 0)
	return errh->error("%s: %s", _filename.cc(), strerror(errno));

    _pos = _len = _file_offset = 0;
    _buffer = String();
    int result = read_buffer(errh);
    if (result < 0)
	return -1;
    else if (result == 0)
	return errh->error("%s: empty file", _filename.cc());

    // check for a gziped or bzip2d dump
    if (_fd == STDIN_FILENO || _pipe)
	/* cannot handle gzip or bzip2 */;
    else if (compressed_data(reinterpret_cast<const unsigned char *>(_buffer.data()), _len)) {
	close(_fd);
	_fd = -1;
	if (!(_pipe = open_uncompress_pipe(_filename, reinterpret_cast<const unsigned char *>(_buffer.data()), _len, errh)))
	    return -1;
	_fd = fileno(_pipe);
	goto retry_file;
    }

    String line;
    if (read_line(line, errh) < 0)
	return -1;
    else
	_pos = 0;
    
    _format_complaint = false;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return 0;
}

void
FromNetFlowSummaryDump::cleanup(CleanupStage)
{
    if (_pipe)
	pclose(_pipe);
    else if (_fd >= 0 && _fd != STDIN_FILENO)
	close(_fd);
    _fd = -1;
    _pipe = 0;
    if (_work_packet)
	_work_packet->kill();
    _work_packet = 0;
    _buffer = String();
}

Packet *
FromNetFlowSummaryDump::read_packet(ErrorHandler *errh)
{
    WritablePacket *q = Packet::make((const char *)0, sizeof(click_ip) + sizeof(click_tcp));
    if (!q) {
	error_helper(errh, "out of memory!");
	return 0;
    }
    if (_zero)
	memset(q->data(), 0, q->length());
    q->set_ip_header((click_ip *)q->data(), sizeof(click_ip));
    click_ip *iph = q->ip_header();
    iph->ip_v = 4;
    iph->ip_hl = sizeof(click_ip) >> 2;
    iph->ip_off = 0;
    
    String line;
    String words[15];
    uint32_t j;
    
    while (1) {

	if (read_line(line, errh) <= 0) {
	    q->kill();
	    return 0;
	}

	const char *data = line.data();
	int len = line.length();

	if (len == 0 || data[0] == '!' || data[0] == '#')
	    continue;

	int pos = 0, dpos = 0;
	while (dpos < len && pos < 15) {
	    int start = dpos;
	    while (dpos < len && data[dpos] != '|')
		dpos++;
	    words[pos++] = line.substring(start, dpos - start);
	    dpos++;
	}
	if (pos < 15)
	    break;

	// relevant indices:
	// 0 - source IP
	// 1 - dest IP
	// 5 - # packets
	// 6 - # bytes
	// 7 - start timestamp sec
	// 8 - end timestamp sec
	// 9 - source port
	// 10 - dest port
	// 13 - protocol
	// 14 - TOS bits

	int ok = 0;
	uint32_t byte_count, end_timestamp;
	ok += cp_ip_address(words[0], (unsigned char *)&iph->ip_src);
	ok += cp_ip_address(words[1], (unsigned char *)&iph->ip_dst);
	if (cp_unsigned(words[5], &j))
	    SET_EXTRA_PACKETS_ANNO(q, j - 1), ok++;
	if (cp_unsigned(words[6], (unsigned *)&byte_count))
	    ok++;
	if (cp_unsigned(words[7], &j))
	    q->set_timestamp_anno(j, 0), ok++;
	if (cp_unsigned(words[7], (unsigned *)&end_timestamp))
	    ok++;
	if (cp_unsigned(words[13], &j) && j <= 0xFF)
	    iph->ip_p = j, ok++;
	if (cp_unsigned(words[14], &j) && j <= 0xFF)
	    iph->ip_tos = j, ok++;
	if (cp_unsigned(words[9], &j) && j <= 0xFFFF)
	    q->udp_header()->uh_sport = htons(j), ok++;
	if (cp_unsigned(words[10], &j) && j <= 0xFFFF)
	    q->udp_header()->uh_dport = htons(j), ok++;

	if (ok < 10)
	    break;
	
	// set TCP offset to a reasonable value; possibly reduce packet length
	if (iph->ip_p == IP_PROTO_TCP)
	    q->tcp_header()->th_off = sizeof(click_tcp) >> 2;
	else if (iph->ip_p == IP_PROTO_UDP)
	    q->take(sizeof(click_tcp) - sizeof(click_udp));
	else
	    q->take(sizeof(click_tcp));
	SET_EXTRA_LENGTH_ANNO(q, byte_count - q->length());
	return q;
    }

    // bad format if we get here
    if (!_format_complaint) {
	error_helper(errh, "bad format");
	_format_complaint = true;
    }
    if (q)
	q->kill();
    return 0;
}

Packet *
FromNetFlowSummaryDump::handle_multipacket(Packet *p)
{
    if (p) {
	uint32_t count = 1 + EXTRA_PACKETS_ANNO(p);
	if (!_work_packet && count > 1)
	    _multipacket_extra_length = EXTRA_LENGTH_ANNO(p) / count;
	_work_packet = (count > 1 ? p : 0);
	if (_work_packet) {
	    SET_EXTRA_PACKETS_ANNO(_work_packet, count - 2);
	    SET_EXTRA_LENGTH_ANNO(_work_packet, EXTRA_LENGTH_ANNO(_work_packet) - _multipacket_extra_length);
	    if ((p = p->clone())) {
		SET_EXTRA_PACKETS_ANNO(p, 0);
		SET_EXTRA_LENGTH_ANNO(p, _multipacket_extra_length);
	    }
	}
    }
    return p;
}

bool
FromNetFlowSummaryDump::run_task()
{
    if (!_active)
	return false;

    Packet *p = (_work_packet ? _work_packet : read_packet(0));
    if (!p) {
	if (_stop)
	    router()->please_stop_driver();
	return false;
    }

    if (_multipacket)
	p = handle_multipacket(p);
    if (p)
	output(0).push(p);
    _task.fast_reschedule();
    return true;
}

Packet *
FromNetFlowSummaryDump::pull(int)
{
    if (!_active) {
	_notifier.sleep_listeners();
	return 0;
    }

    Packet *p = (_work_packet ? _work_packet : read_packet(0));
    _notifier.set_listeners(p != 0);
    if (!p && _stop)
	router()->please_stop_driver();
    if (_multipacket)
	p = handle_multipacket(p);
    return p;
}


enum { H_ACTIVE, H_ENCAP, H_FILESIZE, H_FILEPOS };

String
FromNetFlowSummaryDump::read_handler(Element *e, void *thunk)
{
    FromNetFlowSummaryDump *fd = static_cast<FromNetFlowSummaryDump *>(e);
    switch ((intptr_t)thunk) {
      case H_ACTIVE:
	return cp_unparse_bool(fd->_active) + "\n";
      case H_ENCAP:
	return "IP\n";
      case H_FILESIZE: {
	  struct stat s;
	  if (fd->_fd >= 0 && fstat(fd->_fd, &s) >= 0 && S_ISREG(s.st_mode))
	      return String(s.st_size) + "\n";
	  else
	      return "-\n";
      }
      case H_FILEPOS:
	return String(fd->_file_offset + fd->_pos) + "\n";
      default:
	return "<error>\n";
    }
}

int
FromNetFlowSummaryDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromNetFlowSummaryDump *fd = static_cast<FromNetFlowSummaryDump *>(e);
    String s = cp_uncomment(s_in);
    switch ((intptr_t)thunk) {
      case H_ACTIVE: {
	  bool active;
	  if (cp_bool(s, &active)) {
	      fd->_active = active;
	      if (fd->output_is_push(0)) {
		  if (active && !fd->_task.scheduled())
		      fd->_task.reschedule();
	      } else
		  fd->_notifier.set_listeners(active);
	      return 0;
	  } else
	      return errh->error("`active' should be Boolean");
      }
      default:
	return -EINVAL;
    }
}

void
FromNetFlowSummaryDump::add_handlers()
{
    add_read_handler("active", read_handler, (void *)H_ACTIVE);
    add_write_handler("active", write_handler, (void *)H_ACTIVE);
    add_read_handler("encap", read_handler, (void *)H_ENCAP);
    add_read_handler("filesize", read_handler, (void *)H_FILESIZE);
    add_read_handler("filepos", read_handler, (void *)H_FILEPOS);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromNetFlowSummaryDump)
