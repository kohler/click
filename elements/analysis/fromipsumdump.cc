// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromipsumdump.{cc,hh} -- element reads packets from IP summary dump file
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

#include "fromipsumdump.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/click_ip.h>
#include <click/click_udp.h>
#include <click/click_tcp.h>
#include <click/packet_anno.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static uint8_t flag_mapping[256];

FromIPSummaryDump::FromIPSummaryDump()
    : Element(0, 1), _fd(-1), _buffer(0), _pos(0), _len(0), _buffer_len(0),
      _work_packet(0), _task(this), _pipe(0)
{
    MOD_INC_USE_COUNT;
    if (!flag_mapping[(uint8_t)'A']) {
	const char *x = tcp_flags_word;
	for (int i = 0; *x; x++, i++)
	    flag_mapping[(uint8_t)(*x)] = i + 1;
    }
}

FromIPSummaryDump::~FromIPSummaryDump()
{
    MOD_DEC_USE_COUNT;
}

int
FromIPSummaryDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true, zero = false, multipacket = false;
    uint8_t default_proto = IP_PROTO_TCP;
    _sampling_prob = (1 << SAMPLING_SHIFT);
    String default_contents;
    
    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump file name", &_filename,
		    cpKeywords,
		    "STOP", cpBool, "stop driver when done?", &stop,
		    "ACTIVE", cpBool, "start active?", &active,
		    "ZERO", cpBool, "zero packet data?", &zero,
		    "SAMPLE", cpUnsignedReal2, "sampling probability", SAMPLING_SHIFT, &_sampling_prob,
		    "PROTO", cpByte, "default IP protocol", &default_proto,
		    "MULTIPACKET", cpBool, "generate multiple packets per record?", &multipacket,
		    "DEFAULT_CONTENTS", cpArgument, "default contents of log", &default_contents,
		    0) < 0)
	return -1;
    if (_sampling_prob > (1 << SAMPLING_SHIFT)) {
	errh->warning("SAMPLE probability reduced to 1");
	_sampling_prob = (1 << SAMPLING_SHIFT);
    } else if (_sampling_prob == 0)
	errh->warning("SAMPLE probability is 0; emitting no packets");

    _default_proto = default_proto;
    _stop = stop;
    _active = active;
    _zero = zero;
    _multipacket = multipacket;
    if (default_contents)
	bang_data(default_contents, errh);
    return 0;
}

int
FromIPSummaryDump::error_helper(ErrorHandler *errh, const char *x)
{
    if (errh)
	errh->error("%s: %s", _filename.cc(), x);
    else
	click_chatter("%s: %s", id().cc(), x);
    return -1;
}

int
FromIPSummaryDump::read_buffer(ErrorHandler *errh)
{
    if (_pos == 0 && _len == _buffer_len) {
	_buffer_len += BUFFER_SIZE;
	if (!(_buffer = (char *)realloc(_buffer, _buffer_len)))
	    return error_helper(errh, strerror(ENOMEM));
    }

    if (_len == _buffer_len) {
	memmove(_buffer, _buffer + _pos, _len - _pos);
	_len -= _pos;
	_file_offset += _pos;
	_pos = 0;
    }
    int initial_len = _len;
    
    while (_len < _buffer_len) {
	ssize_t got = read(_fd, _buffer + _len, _buffer_len - _len);
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
FromIPSummaryDump::read_line(String &result, ErrorHandler *errh)
{
    int epos = _pos;
    if (_save_char)
	_buffer[epos] = _save_char;

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

	while (epos < _len && _buffer[epos] != '\n' && _buffer[epos] != '\r')
	    epos++;

	if (epos < _len || done) {
	    if (epos < _len && _buffer[epos] == '\r')
		epos++;
	    if (epos < _len && _buffer[epos] == '\n')
		epos++;

	    // add terminating '\0'
	    if (epos == _buffer_len) {
		_buffer_len += BUFFER_SIZE;
		if (!(_buffer = (char *)realloc(_buffer, _buffer_len)))
		    return error_helper(errh, strerror(ENOMEM));
	    }
	    _save_char = _buffer[epos];
	    _buffer[epos] = '\0';

	    result = String::stable_string(_buffer + _pos, epos - _pos);
	    _pos = epos;
	    return 1;
	}
    }
}

int
FromIPSummaryDump::initialize(ErrorHandler *errh)
{
    _pipe = 0;
    if (_filename == "-") {
	_fd = STDIN_FILENO;
	_filename = "<stdin>";
    } else
	_fd = open(_filename.cc(), O_RDONLY);

  retry_file:
    if (_fd < 0)
	return errh->error("%s: %s", _filename.cc(), strerror(errno));

    _pos = _len = _file_offset = _save_char = 0;
    int result = read_buffer(errh);
    if (result < 0)
	return -1;
    else if (result == 0)
	return errh->error("%s: empty file", _filename.cc());

    // check for a gziped or bzip2d dump
    if (_fd == STDIN_FILENO || _pipe)
	/* cannot handle gzip or bzip2 */;
    else if (compressed_data(reinterpret_cast<const unsigned char *>(_buffer), _len)) {
	close(_fd);
	_fd = -1;
	if (!(_pipe = open_uncompress_pipe(_filename, reinterpret_cast<const unsigned char *>(_buffer), _len, errh)))
	    return -1;
	_fd = fileno(_pipe);
	goto retry_file;
    }

    String line;
    if (read_line(line, errh) < 0)
	return -1;
    else if (line.substring(0, 14) != "!IPSummaryDump"
	     && line.substring(0, 8) != "!creator") {
	if (!_contents.size() /* don't warn on DEFAULT_CONTENTS */)
	    errh->warning("%s: missing banner line; is this an IP summary dump?", _filename.cc());
	if (_save_char)
	    _buffer[_pos] = _save_char;
	_pos = _save_char = 0;
    }
    
    _format_complaint = false;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return 0;
}

void
FromIPSummaryDump::cleanup(CleanupStage)
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
    free(_buffer);
    _buffer = 0;
}

void
FromIPSummaryDump::bang_data(const String &line, ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(line, words);

    _contents.clear();
    uint32_t all_contents = 0;
    for (int i = 0; i < words.size(); i++) {
	String word = cp_unquote(words[i]);
	int what = parse_content(word);
	if (what > W_NONE && what < W_LAST) {
	    _contents.push_back(what);
	    all_contents |= (1 << (what - W_NONE - 1));
	} else if (i > 0 || word != "!data") {
	    error_helper(errh, "warning: unknown content type `" + word + "'");
	    _contents.push_back(W_NONE);
	}
    }

    if (_contents.size() == 0)
	error_helper(errh, "no contents specified");

    // If we have W_FRAGOFF, ignore W_FRAG.
    if (all_contents & (1 << (W_FRAGOFF - W_NONE - 1)))
	for (int i = 0; i < _contents.size(); i++)
	    if (_contents[i] == W_FRAG)
		_contents[i] = W_NONE;
}

Packet *
FromIPSummaryDump::read_packet(ErrorHandler *errh)
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
    iph->ip_p = _default_proto;
    iph->ip_off = 0;
    
    String line;
    
    while (1) {

	if (read_line(line, errh) <= 0) {
	    q->kill();
	    return 0;
	}

	const char *data = line.data();
	int len = line.length();

	if (len >= 6 && memcmp(data, "!data", 5) == 0 && isspace(data[5])) {
	    bang_data(line, errh);
	    continue;
	} else if (len >= 7 && memcmp(data, "!proto", 6) == 0 && isspace(data[6])) {
	    //handle_proto_line(line, errh);
	    continue;
	} else if (len == 0 || data[0] == '!' || data[0] == '#')
	    continue;

	int ok = 0;
	int pos = 0;
	uint32_t byte_count = 0;
	uint32_t payload_len = 0;
	bool have_payload_len = false;
	
	for (int i = 0; pos < len && i < _contents.size(); i++) {
	    int original_pos = pos;
	    char *next;
	    uint32_t u1 = 0, u2 = 0;

	    // first, parse contents
	    switch (_contents[i]) {

	      case W_TIMESTAMP:
		u1 = strtoul(data + pos, &next, 10);
		if (next > data + pos) {
		    pos = next - data;
		    if (data[pos] == '.') {
			int digit = 0;
			for (pos++; digit < 6 && isdigit(data[pos]); digit++, pos++)
			    u2 = (u2 * 10) + data[pos] - '0';
			for (; digit < 6; digit++)
			    u2 = (u2 * 10);
			for (; isdigit(data[pos]); pos++)
			    /* nada */;
		    }
		}
		break;
		
	      case W_TIMESTAMP_SEC:
	      case W_TIMESTAMP_USEC:
	      case W_LENGTH:
	      case W_PAYLOAD_LENGTH:
	      case W_IPID:
	      case W_SPORT:
	      case W_DPORT:
	      case W_TCP_SEQ:
	      case W_TCP_ACK:
	      case W_COUNT:
		u1 = strtoul(data + pos, &next, 0);
		pos = next - data;
		break;
		
	      case W_SRC:
	      case W_DST:
		for (int j = 0; j < 4; j++) {
		    int x = 0, p = pos;
		    while (isdigit(data[pos]) && x < 256)
			(x = (x * 10) + data[pos] - '0'), pos++;
		    if (x >= 256 || pos == p || (j < 3 && data[pos] != '.')) {
			pos = original_pos;
			break;
		    }
		    u1 = (u1 << 8) + x;
		    if (j < 3)
			pos++;
		}
		break;

	      case W_PROTO:
		if (data[pos] == 'T') {
		    u1 = IP_PROTO_TCP;
		    pos++;
		} else if (data[pos] == 'U') {
		    u1 = IP_PROTO_UDP;
		    pos++;
		} else if (data[pos] == 'I') {
		    u1 = IP_PROTO_ICMP;
		    pos++;
		} else {
		    u1 = strtoul(data + pos, &next, 0);
		    pos = next - data;
		}
		break;

	      case W_FRAG:
		if (data[pos] == 'F') {
		    u1 = htons(IP_MF);
		    pos++;
		} else if (data[pos] == 'f') {
		    u1 = htons(10);	// random number
		    pos++;
		} else if (data[pos] == '.')
		    pos++;	// u1 already 0
		break;

	      case W_FRAGOFF:
		u1 = strtoul(data + pos, &next, 0);
		if (next > data + pos) {
		    pos = next - data;
		    if (data[pos] == '+') {
			u1 |= IP_MF;
			pos++;
		    }
		}
		break;

	      case W_TCP_FLAGS:
		if (isdigit(data[pos])) {
		    u1 = strtoul(data + pos, &next, 0);
		    pos = next - data;
		} else if (data[pos] == '.')
		    pos++;
		else
		    while (flag_mapping[data[pos]]) {
			u1 |= 1 << (flag_mapping[data[pos]] - 1);
			pos++;
		    }
		break;
		
	      case W_LINK:
		if (data[pos] == 'L' || data[pos] == '>') {
		    u1 = 0;
		    pos++;
		} else if (data[pos] == 'R' || data[pos] == 'X' || data[pos] == '<') {
		    u1 = 1;
		    pos++;
		} else {
		    u1 = strtoul(data + pos, &next, 0);
		    pos = next - data;
		}
		break;

	    }

	    // check whether we correctly parsed something
	    bool this_ok = (pos > original_pos && (!data[pos] || isspace(data[pos])));
	    while (data[pos] && !isspace(data[pos]))
		pos++;
	    while (isspace(data[pos]))
		pos++;
	    if (!this_ok)
		continue;

	    // store contents
	    switch (_contents[i]) {

	      case W_TIMESTAMP:
		if (u2 < 1000000)
		    q->set_timestamp_anno(u1, u2), ok++;
		break;

	      case W_TIMESTAMP_SEC:
		q->timestamp_anno().tv_sec = u1, ok++;
		break;

	      case W_TIMESTAMP_USEC:
		if (u1 < 1000000)
		    q->timestamp_anno().tv_usec = u1, ok++;
		break;
		
	      case W_SRC:
		iph->ip_src.s_addr = htonl(u1), ok++;
		break;

	      case W_DST:
		iph->ip_dst.s_addr = htonl(u1), ok++;
		break;
		
	      case W_LENGTH:
		if (u1 <= 0xFFFF)
		    byte_count = u1, ok++;
		break;
		
	      case W_PAYLOAD_LENGTH:
		if (u1 <= 0xFFFF)
		    payload_len = u1, have_payload_len = true, ok++;
		break;
		
	      case W_PROTO:
		if (u1 <= 255)
		    iph->ip_p = u1, ok++;
		break;

	      case W_IPID:
		if (u1 <= 0xFFFF)
		    iph->ip_id = htons(u1), ok++;
		break;

	      case W_FRAG:
		iph->ip_off = u1, ok++;
		break;

	      case W_FRAGOFF:
		if ((u1 & ~IP_MF) <= IP_OFFMASK)
		    iph->ip_off = htons(u1), ok++;
		break;

	      case W_SPORT:
		if (u1 <= 0xFFFF)
		    q->udp_header()->uh_sport = htons(u1), ok++;
		break;

	      case W_DPORT:
		if (u1 <= 0xFFFF)
		    q->udp_header()->uh_dport = htons(u1), ok++;
		break;

	      case W_TCP_SEQ:
		q->tcp_header()->th_seq = htonl(u1), ok++;
		break;

	      case W_TCP_ACK:
		q->tcp_header()->th_ack = htonl(u1), ok++;
		break;

	      case W_TCP_FLAGS:
		if (u1 <= 0xFF)
		    q->tcp_header()->th_flags = u1, ok++;
		break;

	      case W_COUNT:
		if (u1)
		    SET_EXTRA_PACKETS_ANNO(q, u1 - 1), ok++;
		break;

	      case W_LINK:
		SET_PAINT_ANNO(q, u1), ok++;
		break;

	    }
	}

	if (!ok)
	    break;

	// set TCP offset to a reasonable value; possibly reduce packet length
	if (iph->ip_p == IP_PROTO_TCP && IP_FIRSTFRAG(iph))
	    q->tcp_header()->th_off = sizeof(click_tcp) >> 2;
	else if (iph->ip_p == IP_PROTO_UDP && IP_FIRSTFRAG(iph))
	    q->take(sizeof(click_tcp) - sizeof(click_udp));
	else
	    q->take(sizeof(click_tcp));
	if (byte_count) {
	    iph->ip_len = ntohs(byte_count);
	    SET_EXTRA_LENGTH_ANNO(q, byte_count - q->length());
	} else if (have_payload_len) {
	    iph->ip_len = ntohs(q->length() + payload_len);
	    SET_EXTRA_LENGTH_ANNO(q, payload_len);
	}

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
FromIPSummaryDump::handle_multipacket(Packet *p)
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

void
FromIPSummaryDump::run_scheduled()
{
    if (!_active)
	return;
    Packet *p;

    while (1) {
	p = (_work_packet ? _work_packet : read_packet(0));
	if (!p) {
	    if (_stop)
		router()->please_stop_driver();
	    return;
	}
	if (_multipacket)
	    p = handle_multipacket(p);
	// check sampling probability
	if (_sampling_prob >= (1 << SAMPLING_SHIFT)
	    || (uint32_t)(random() & ((1 << SAMPLING_SHIFT) - 1)) < _sampling_prob)
	    break;
	if (p)
	    p->kill();
    }

    if (p)
	output(0).push(p);
    _task.fast_reschedule();
}

Packet *
FromIPSummaryDump::pull(int)
{
    if (!_active)
	return 0;
    Packet *p;

    while (1) {
	p = (_work_packet ? _work_packet : read_packet(0));
	if (!p) {
	    if (_stop)
		router()->please_stop_driver();
	    return 0;
	}
	if (_multipacket)
	    p = handle_multipacket(p);
	// check sampling probability
	if (_sampling_prob >= (1 << SAMPLING_SHIFT)
	    || (uint32_t)(random() & ((1 << SAMPLING_SHIFT) - 1)) < _sampling_prob)
	    break;
	if (p)
	    p->kill();
    }
    
    return p;
}


enum { H_SAMPLING_PROB, H_ACTIVE, H_ENCAP, H_FILESIZE, H_FILEPOS, H_STOP };

String
FromIPSummaryDump::read_handler(Element *e, void *thunk)
{
    FromIPSummaryDump *fd = static_cast<FromIPSummaryDump *>(e);
    switch ((int)thunk) {
      case H_SAMPLING_PROB:
	return cp_unparse_real2(fd->_sampling_prob, SAMPLING_SHIFT) + "\n";
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
FromIPSummaryDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromIPSummaryDump *fd = static_cast<FromIPSummaryDump *>(e);
    String s = cp_uncomment(s_in);
    switch ((int)thunk) {
      case H_ACTIVE: {
	  bool active;
	  if (cp_bool(s, &active)) {
	      fd->_active = active;
	      if (active && fd->output_is_push(0) && !fd->_task.scheduled())
		  fd->_task.reschedule();
	      return 0;
	  } else
	      return errh->error("`active' should be Boolean");
      }
      case H_STOP:
	fd->_active = false;
	fd->router()->please_stop_driver();
	return 0;
      default:
	return -EINVAL;
    }
}

void
FromIPSummaryDump::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, (void *)H_SAMPLING_PROB);
    add_read_handler("active", read_handler, (void *)H_ACTIVE);
    add_write_handler("active", write_handler, (void *)H_ACTIVE);
    add_read_handler("encap", read_handler, (void *)H_ENCAP);
    add_read_handler("filesize", read_handler, (void *)H_FILESIZE);
    add_read_handler("filepos", read_handler, (void *)H_FILEPOS);
    add_write_handler("stop", write_handler, (void *)H_STOP);
    if (output_is_push(0))
	add_task_handlers(&_task);
}


static const char *content_names[] = {
    "??", "timestamp", "ts sec", "ts usec",
    "ip src", "ip dst", "ip len", "ip proto", "ip id",
    "sport", "dport", "tcp seq", "tcp ack", "tcp flags",
    "payload len", "count", "ip frag", "ip fragoff",
    "payload", "direction"
};

const char *
FromIPSummaryDump::unparse_content(int what)
{
    if (what < 0 || what >= (int)(sizeof(content_names) / sizeof(content_names[0])))
	return "??";
    else
	return content_names[what];
}

int
FromIPSummaryDump::parse_content(const String &word)
{
    if (word == "timestamp" || word == "ts")
	return W_TIMESTAMP;
    else if (word == "sec" || word == "ts sec")
	return W_TIMESTAMP_SEC;
    else if (word == "usec" || word == "ts usec")
	return W_TIMESTAMP_USEC;
    else if (word == "src" || word == "ip src")
	return W_SRC;
    else if (word == "dst" || word == "ip dst")
	return W_DST;
    else if (word == "sport")
	return W_SPORT;
    else if (word == "dport")
	return W_DPORT;
    else if (word == "frag" || word == "ip frag")
	return W_FRAG;
    else if (word == "fragoff" || word == "ip fragoff")
	return W_FRAGOFF;
    else if (word == "len" || word == "length" || word == "ip len")
	return W_LENGTH;
    else if (word == "id" || word == "ip id")
	return W_IPID;
    else if (word == "proto" || word == "ip proto")
	return W_PROTO;
    else if (word == "tcp seq" || word == "tcp seqno")
	return W_TCP_SEQ;
    else if (word == "tcp ack" || word == "tcp ackno")
	return W_TCP_ACK;
    else if (word == "tcp flags")
	return W_TCP_FLAGS;
    else if (word == "payload len" || word == "payload length")
	return W_PAYLOAD_LENGTH;
    else if (word == "count" || word == "pkt count" || word == "packet count")
	return W_COUNT;
    else if (word == "payload")
	return W_PAYLOAD;
    else if (word == "link" || word == "direction")
	return W_LINK;
    else
	return W_NONE;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromIPSummaryDump)
