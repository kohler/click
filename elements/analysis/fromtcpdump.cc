// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromtcpdump.{cc,hh} -- element reads packets from IP summary dump file
 * Eddie Kohler
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

#include "fromtcpdump.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <click/packet_anno.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
CLICK_DECLS

FromTcpdump::FromTcpdump()
    : Element(0, 1), _fd(-1), _buffer(0), _pos(0), _len(0), _buffer_len(0),
      _task(this), _pipe(0)
{
    MOD_INC_USE_COUNT;
}

FromTcpdump::~FromTcpdump()
{
    MOD_DEC_USE_COUNT;
}

void *
FromTcpdump::cast(const char *n)
{
    if (strcmp(n, "Notifier") == 0 && !output_is_push(0)) {
	_notifier.initialize(router());
	return static_cast<Notifier *>(&_notifier);
    } else
	return Element::cast(n);
}

int
FromTcpdump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true, zero = false, checksum = false;
    _sampling_prob = (1 << SAMPLING_SHIFT);
    _absolute_seq = -1;
    
    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump file name", &_filename,
		    cpKeywords,
		    "STOP", cpBool, "stop driver when done?", &stop,
		    "ACTIVE", cpBool, "start active?", &active,
		    "ZERO", cpBool, "zero packet data?", &zero,
		    "CHECKSUM", cpBool, "set packet checksums?", &checksum,
		    "SAMPLE", cpUnsignedReal2, "sampling probability", SAMPLING_SHIFT, &_sampling_prob,
		    0) < 0)
	return -1;
    if (_sampling_prob > (1 << SAMPLING_SHIFT)) {
	errh->warning("SAMPLE probability reduced to 1");
	_sampling_prob = (1 << SAMPLING_SHIFT);
    } else if (_sampling_prob == 0)
	errh->warning("SAMPLE probability is 0; emitting no packets");

    _stop = stop;
    _active = active;
    _zero = zero;
    _checksum = checksum;
    _dead = false;
    return 0;
}

int
FromTcpdump::error_helper(ErrorHandler *errh, const char *x)
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    return errh->error("%s:%d: %s", _filename.cc(), _lineno, x);
}

int
FromTcpdump::read_buffer(ErrorHandler *errh)
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
FromTcpdump::read_line(String &result, ErrorHandler *errh)
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
	    _lineno++;
	    return 1;
	}
    }
}

int
FromTcpdump::initialize(ErrorHandler *errh)
{
    // make sure notifier is initialized
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

    _pos = _len = _file_offset = _save_char = _lineno = 0;
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

    // read a line
    String line;
    if (read_line(line, errh) < 0)
	return -1;
    else if (!line || !isdigit(line[0]))
	errh->warning("%s: first line suspicious; is this a tcpdump output file?", _filename.c_str());
    // patch line
    if (_save_char)
	_buffer[_pos] = _save_char;
    _pos = _save_char = _lineno = 0;
    
    _format_complaint = false;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return 0;
}

void
FromTcpdump::cleanup(CleanupStage)
{
    if (_pipe)
	pclose(_pipe);
    else if (_fd >= 0 && _fd != STDIN_FILENO)
	close(_fd);
    _fd = -1;
    _pipe = 0;
    free(_buffer);
    _buffer = 0;
}

#if 0
static void
append_net_uint32_t(StringAccum &sa, uint32_t u)
{
    sa << (char)(u >> 24) << (char)(u >> 16) << (char)(u >> 8) << (char)u;
}
#endif

static void
set_checksums(WritablePacket *q, click_ip *iph)
{
    assert(iph == q->ip_header());
    
    iph->ip_sum = 0;
    iph->ip_sum = click_in_cksum((uint8_t *)iph, iph->ip_hl << 2);

    if (IP_ISFRAG(iph))
	/* nada */;
    else if (iph->ip_p == IP_PROTO_TCP) {
	click_tcp *tcph = q->tcp_header();
	tcph->th_sum = 0;
	unsigned csum = click_in_cksum((uint8_t *)tcph, q->transport_length());
	tcph->th_sum = click_in_cksum_pseudohdr(csum, iph->ip_src.s_addr, iph->ip_dst.s_addr, IP_PROTO_TCP, q->transport_length());
    } else if (iph->ip_p == IP_PROTO_UDP) {
	click_udp *udph = q->udp_header();
	udph->uh_sum = 0;
	unsigned csum = click_in_cksum((uint8_t *)udph, q->transport_length());
	udph->uh_sum = click_in_cksum_pseudohdr(csum, iph->ip_src.s_addr, iph->ip_dst.s_addr, IP_PROTO_UDP, q->transport_length());
    }
}

const char *
FromTcpdump::read_tcp_line(WritablePacket *&q, const String &line, const char *s, int *data_len)
{
    click_tcp *tcph = q->tcp_header();
    const char *data = line.data();

    // first, read flags
    if (*s == '.')
	tcph->th_flags = TH_ACK, s++;
    else {
	tcph->th_flags = 0;
	while (IPSummaryDumpInfo::tcp_flag_mapping[(uint8_t) *s]) {
	    tcph->th_flags |= (1 << (IPSummaryDumpInfo::tcp_flag_mapping[ (uint8_t) *s ] - 1));
	    s++;
	}
    }
    if (*s != ' ')
	return s;

    // second, check for '[bad hdr length]' '[tcp sum ok]'
    const char *s2;
    if (s[1] == '[' && memcmp(s + 2, "bad hdr", 7) == 0 && (s2 = strchr(s + 9, ']')) && s2[1] == ' ')
	s = s2 + 2;
    if (s[1] == '[' && memcmp(s + 2, "tcp sum ok] ", 12) == 0)
	s += 14;
    else if (s[1] == '[' && memcmp(s + 2, "bad tcp", 7) == 0 && (s2 = strchr(s + 9, ']')) && s2[1] == ' ')
	s = s2 + 2;
    else
	s++;
    
    // then read sequence numbers
    uint32_t seq = 0, end_seq = 0, ack_seq = 0;
    if (s[0] != 'a') {
	const char *colon = strchr(s, ':');
	if (!colon || colon == s || !isdigit(s[0]) || !isdigit(colon[1]))
	    return s;
	const char *paren = strchr(colon + 2, '(');
	if (!paren)
	    return s;
	if (!cp_unsigned(line.substring(s - data, colon - s), &seq)
	    || !cp_unsigned(line.substring(colon + 1 - data, paren - colon - 1), &end_seq))
	    return s;
	
	// then skip the parenthesized length
	for (paren++; isdigit(*paren); paren++)
	    ;
	if (*paren != ')')
	    return paren;
	else if (paren[1] != ' ')
	    return paren + 1;
	s = paren + 2;
    }
    *data_len = end_seq - seq;

    // check for 'ack'
    if (s[0] == 'a' && s[1] == 'c' && s[2] == 'k' && s[3] == ' ' && isdigit(s[4])) {
	for (s2 = s + 5; isdigit(*s2); s2++)
	    ;
	cp_unsigned(line.substring(s + 4 - data, s2 - s - 4), &ack_seq);
	tcph->th_flags |= TH_ACK;
	s = (*s2 == ' ' ? s2 + 1 : s2);
    }

    // patch sequence numbers
    if (tcph->th_flags & TH_ACK) {
	// first, look up a record for this flow
	FlowRecord *record;
	const click_ip *iph = q->ip_header();
	bool rev = (tcph->th_sport < tcph->th_dport
		    || (tcph->th_sport == tcph->th_dport && iph->ip_src.s_addr < iph->ip_dst.s_addr));
	if (rev)
	    record = _tcp_map.findp_force(IPFlowID(iph->ip_src, tcph->th_sport, iph->ip_dst, tcph->th_dport));
	else
	    record = _tcp_map.findp_force(IPFlowID(iph->ip_dst, tcph->th_dport, iph->ip_src, tcph->th_sport));
	// use tcpdump's heuristic for deciding when this is a new flow
	if ((!record->init_seq[0] && !record->init_seq[1]) || (tcph->th_flags & TH_SYN)) {
	    record->init_seq[rev] = seq;
	    record->init_seq[!rev] = ack_seq - 1;
	} else {
	    if (_absolute_seq < 0) // heuristic
		_absolute_seq = (ack_seq >= record->init_seq[!rev] || ack_seq > 8000);
	    if (seq == 0 && end_seq == 0)
		seq = end_seq = record->last_seq[rev];
	    else if (!_absolute_seq) {
		seq += record->init_seq[rev];
		end_seq += record->init_seq[rev];
	    }
	    if (!_absolute_seq)
		ack_seq += record->init_seq[!rev];
	}
	// record last seen sequence numbers for assignment to pure acks
	record->last_seq[rev] = end_seq + (tcph->th_flags & TH_SYN ? 1 : 0) + (tcph->th_flags & TH_FIN ? 1 : 0);
	if (!record->last_seq[!rev])
	    record->last_seq[!rev] = ack_seq;
    }
    tcph->th_seq = htonl(seq);
    tcph->th_ack = htonl(ack_seq);
    
    // check for 'win'
    uint32_t u;
    if (s[0] == 'w' && s[1] == 'i' && s[2] == 'n' && s[3] == ' ' && isdigit(s[4])) {
	for (s2 = s + 5; isdigit(*s2); s2++)
	    ;
	cp_unsigned(line.substring(s + 4 - data, s2 - s - 4), &u);
	if (u <= 65535) {
	    tcph->th_win = htons(u);
	    s = (*s2 == ' ' ? s2 + 1 : s2);
	}
    }

    // check for 'urg'
    if (s[0] == 'u' && s[1] == 'r' && s[2] == 'g' && s[3] == ' ' && isdigit(s[4])) {
	for (s2 = s + 5; isdigit(*s2); s2++)
	    ;
	cp_unsigned(line.substring(s + 4 - data, s2 - s - 4), &u);
	if (u <= 65535) {
	    tcph->th_urp = htons(u);
	    s = (*s2 == ' ' ? s2 + 1 : s2);
	}
    }

    // check for options
    StringAccum opt;
    if (s[0] == '<') {
	for (s++; *s && *s != '>'; s += (*s == ',' ? 1 : 0)) {
	    if (s[0] == 'n' && s[1] == 'o' && s[2] == 'p' && !isalpha(s[3])) {
		opt << (char)TCPOPT_NOP;
		s += 3;
	    } else if (s[0] == 'e' && s[1] == 'o' && s[2] == 'l' && !isalpha(s[3])) {
		opt << (char)TCPOPT_EOL;
		s += 3;
	    } else if (s[0] == 'm' && s[1] == 's' && s[2] == 's' && s[3] == ' ' && isdigit(s[4])) {
		for (s2 = s + 4; isdigit(*s2); s2++)
		    ;
		cp_unsigned(line.substring(s + 4 - data, s2 - s - 4), &u);
		if (u <= 65535)
		    opt << (char)TCPOPT_MAXSEG << (char)TCPOLEN_MAXSEG << (char)((u >> 8) & 255) << (char)(u & 255);
		s = s2;
	    } else if (s[0] == 'w' && s[1] == 's' && s[2] == 'c' && s[3] == 'a' && s[4] == 'l' && s[5] == 'e' && s[6] == ' ' && isdigit(s[7])) {
		for (s2 = s + 7; isdigit(*s2); s2++)
		    ;
		cp_unsigned(line.substring(s + 7 - data, s2 - s - 7), &u);
		if (u <= 255)
		    opt << (char)TCPOPT_WSCALE << (char)TCPOLEN_WSCALE << (char)u;
		s = s2;
	    } else if (s[0] == 's' && s[1] == 'a' && s[2] == 'c' && s[3] == 'k' && s[4] == 'O' && s[5] == 'K' && !isalpha(s[6])) {
		opt << (char)TCPOPT_SACK_PERMITTED << (char)TCPOLEN_SACK_PERMITTED;
		s += 6;
	    } else if (s[0] == 't' && s[1] == 'i' && s[2] == 'm' && s[3] == 'e' && s[4] == 's' && s[5] == 't' && s[6] == 'a' && s[7] == 'm' && s[8] == 'p' && s[9] == ' ' && isdigit(s[10])) {
		for (s2 = s + 10; isdigit(*s2); s2++)
		    ;
		cp_unsigned(line.substring(s + 10 - data, s2 - s - 10), &u);
		const char *s3 = s2;
		if (*s2 == ' ' && isdigit(s2[1])) {
		    for (s3 += 2; isdigit(*s3); s3++)
			;
		    uint32_t u2;
		    cp_unsigned(line.substring(s2 + 1 - data, s3 - s2 - 1), &u2);
		    opt << (char)TCPOPT_TIMESTAMP << (char)TCPOLEN_TIMESTAMP << (char)((u >> 24) & 255) << (char)((u >> 16) & 255) << (char)((u >> 8) & 255) << (char)(u & 255) << (char)((u2 >> 24) & 255) << (char)((u2 >> 16) & 255) << (char)((u2 >> 8) & 255) << (char)(u2 & 255);
		}
		s = s3;
	    } else {
		while (*s && *s != ',' && *s != '>')
		    s++;
	    }
	}
	if (*s == '>')
	    s++;
	while (opt.length() % 4 != 0)
	    opt << (char)TCPOPT_EOL;
	q = q->put(opt.length());
	memcpy(q->transport_header() + sizeof(click_tcp), opt.data(), opt.length());
	q->tcp_header()->th_off = (sizeof(click_tcp) + opt.length()) >> 2;
    } else
	tcph->th_off = sizeof(click_tcp) >> 2;

    return s;
}

Packet *
FromTcpdump::read_packet(ErrorHandler *errh)
{
    WritablePacket *q = Packet::make(0, (const unsigned char *)0, sizeof(click_ip) + sizeof(click_tcp), 20);
    if (!q) {
	error_helper(errh, "out of memory!");
	_dead = true;
	return 0;
    }
    if (_zero)
	memset(q->data(), 0, q->length());
    q->set_ip_header((click_ip *)q->data(), sizeof(click_ip));
    click_ip *iph = q->ip_header();
    iph->ip_v = 4;
    iph->ip_hl = sizeof(click_ip) >> 2;
    iph->ip_off = 0;
    iph->ip_tos = 0;
    click_udp *udph = q->udp_header();
    
    String line;
    StringAccum payload;
    String ip_opt;
    String tcp_opt;
    
    while (1) {

	if (read_line(line, errh) <= 0) {
	    q->kill();
	    _dead = true;
	    return 0;
	}

	const char *data = line.data();
	if (data[0] == 0 || data[0] == '#')
	    continue;
	else if (!isdigit(data[0]))
	    break;

	// first, read timestamp
	const char *s = data;
	const char *s2 = strchr(s, ' ');
	if (!s2 || !cp_timeval(line.substring(0, s2 - s), &q->timestamp_anno()))
	    break;
	s = s2 + 1;

	// then, guess protocol
	iph->ip_p = 0;
	const char *colon = strchr(s, ':');
	if (colon) {
	    colon += (colon[1] == ' ' ? 2 : 1);
	    if (colon[0] == 'i')
		iph->ip_p = IP_PROTO_ICMP;
	    else if (colon[0] == 'u' || (colon[0] == '[' && colon[1] == 'u'))
		iph->ip_p = IP_PROTO_UDP;
	    else if (colon[0] == '.' || (colon[0] >= 'A' && colon[0] <= 'Z'))
		iph->ip_p = IP_PROTO_TCP;
	}
	
	// then, read source IP address and port
	s2 = strchr(s, ' ');
	if (!s2 || s2 == s || s2[1] != '>' || s2[2] != ' ')
	    break;
	if (iph->ip_p == IP_PROTO_TCP || iph->ip_p == IP_PROTO_UDP) {
	    const char *sm = s2 - 1;
	    while (sm > s && isdigit(*sm))
		sm--;
	    if (sm == s || (sm[0] != '.' && sm[0] != ':'))
		break;
	    unsigned port;
	    if (!cp_ip_address(line.substring(s - data, sm - s), (IPAddress *) &iph->ip_src)
		|| !cp_unsigned(line.substring(sm + 1 - data, s2 - sm - 1), &port)
		|| port > 65535)
		break;
	    else
		udph->uh_sport = htons(port);
	} else if (!cp_ip_address(line.substring(s - data, s2 - s), (IPAddress *) &iph->ip_src))
	    break;
	s = s2 + 3;

	// then, read destination IP address and port
	s2 = strchr(s, ':');
	if (!s2)
	    s2 = strchr(s, '\0');
	if (iph->ip_p == IP_PROTO_TCP || iph->ip_p == IP_PROTO_UDP) {
	    const char *sm = s2 - 1;
	    while (sm > s && isdigit(*sm))
		sm--;
	    if (sm == s || (sm[0] != '.' && sm[0] != ':'))
		break;
	    unsigned port;
	    if (!cp_ip_address(line.substring(s - data, sm - s), (IPAddress *) &iph->ip_dst)
		|| !cp_unsigned(line.substring(sm + 1 - data, s2 - sm - 1), &port)
		|| port > 65535)
		break;
	    else
		udph->uh_dport = htons(port);
	} else if (!cp_ip_address(line.substring(s - data, s2 - s), (IPAddress *) &iph->ip_dst))
	    break;

	// then, read protocol data
	int data_len = 0;
	if (iph->ip_p == IP_PROTO_TCP && IP_FIRSTFRAG(iph)) {
	    s = read_tcp_line(q, line, colon, &data_len);
	    iph = q->ip_header();
	} else if (iph->ip_p == IP_PROTO_UDP && IP_FIRSTFRAG(iph)) {
	    q->take(sizeof(click_tcp) - sizeof(click_udp));
	    s = colon + 1;
	} else {
	    q->take(sizeof(click_tcp));
	    s = colon + 1;
	}

	// parse IP stuff at the end of the line
	// TTL and ID
	s2 = data + line.length() - 1;
	while (s2 > s) {
	    while (s2 > s && isspace(*s2))
		s2--;
	    if (s2 <= s || (*s2 != ')' && *s2 != ']'))
		break;
	    char opener = (*s2 == ')' ? '(' : '[');
	    const char *open = s2 - 1;
	    while (open >= s && *open != opener)
		open--;
	    const char *close = s2;
	    s2 = open - 1;
	    
	    if (open >= s && open < close) {
		const char *item = open + 1;
		while (item < close) {
		    if (close - item >= 7 && memcmp(item, "tos 0x", 6) == 0)
			iph->ip_tos = strtol(item + 6, (char **) &item, 16);
		    else if (close - item >= 6 && memcmp(item, "ECT(", 4) == 0 && (item[4] == '0' || item[4] == '1') && item[5] == ')') {
			iph->ip_tos = (iph->ip_tos & ~IP_ECNMASK) | (item[4] == '0' ? IP_ECN_ECT1 : IP_ECN_ECT2);
			item += 6;
		    } else if (close - item >= 2 && item[0] == 'C' && item[1] == 'E') {
			iph->ip_tos = (iph->ip_tos & ~IP_ECNMASK) | IP_ECN_CE;
			item += 2;
		    } else if (close - item >= 2 && item[0] == 'D' && item[1] == 'F') {
			iph->ip_off |= htons(IP_DF);
			item += 2;
		    } else if (close - item >= 10 && memcmp(item, "frag ", 5) == 0 && isdigit(item[5])) {
			iph->ip_id = htons(strtol(item + 5, (char **) &item, 0));
			if (item > close - 2 || *item != ':' || !isdigit(item[1]))
			    break;
			data_len = strtol(item + 1, (char **) &item, 0);
			if (item > close - 2 || *item != '@' || !isdigit(item[1]))
			    break;
			iph->ip_off = (iph->ip_off & htons(~IP_OFFMASK)) | htons(strtol(item + 1, (char **) &item, 0));
			if (item < close && *item == '+')
			    iph->ip_off |= htons(IP_MF), item++;
		    } else if (close - item >= 5 && memcmp(item, "ttl ", 4) == 0 && isdigit(item[4]))
			iph->ip_ttl = strtol(item + 4, (char **) &item, 0);
		    else if (close - item >= 4 && memcmp(item, "id ", 3) == 0 && isdigit(item[3]))
			iph->ip_id = htons(strtol(item + 3, (char **) &item, 0));
		    else if (close - item >= 5 && memcmp(item, "len ", 4) == 0 && isdigit(item[4]))
			data_len = strtol(item + 4, (char **) &item, 0) - q->length();
		    else
			break;
		    while (item < close && (*item == ',' || isspace(*item)))
			item++;
		}
	    }
	}
	
	// set IP length
	iph->ip_len = ntohs(q->length() + data_len);

	// set UDP length (after IP length is available)
	if (iph->ip_p == IP_PROTO_UDP && IP_FIRSTFRAG(iph))
	    q->udp_header()->uh_ulen = htons(ntohs(iph->ip_len) - (iph->ip_hl << 2));
	
	// set checksum
	if (_checksum)
	    set_checksums(q, iph);
	
	return q;
    }

    // bad format if we get here
    if (!_format_complaint) {
	// don't complain if the line was all blank
	if ((int) strspn(line.data(), " \t\n\r") != line.length()) {
	    error_helper(errh, "packet parse error");
	    _format_complaint = true;
	}
    }
    if (q)
	q->kill();
    return 0;
}

bool
FromTcpdump::run_task()
{
    if (!_active)
	return false;
    Packet *p;

    while (1) {
	p = read_packet(0);
	if (_dead) {
	    if (_stop)
		router()->please_stop_driver();
	    return false;
	}
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
    return true;
}

Packet *
FromTcpdump::pull(int)
{
    if (!_active)
	return 0;
    Packet *p;

    while (1) {
	p = read_packet(0);
	if (_dead) {
	    if (_stop)
		router()->please_stop_driver();
	    _notifier.set_listeners(false);
	    return 0;
	}
	// check sampling probability
	if (_sampling_prob >= (1 << SAMPLING_SHIFT)
	    || (uint32_t)(random() & ((1 << SAMPLING_SHIFT) - 1)) < _sampling_prob)
	    break;
	if (p)
	    p->kill();
    }

    _notifier.set_listeners(true);
    return p;
}


enum { H_SAMPLING_PROB, H_ACTIVE, H_ENCAP, H_FILENAME, H_FILESIZE, H_FILEPOS, H_STOP };

String
FromTcpdump::read_handler(Element *e, void *thunk)
{
    FromTcpdump *fd = static_cast<FromTcpdump *>(e);
    switch ((intptr_t)thunk) {
      case H_SAMPLING_PROB:
	return cp_unparse_real2(fd->_sampling_prob, SAMPLING_SHIFT) + "\n";
      case H_ACTIVE:
	return cp_unparse_bool(fd->_active) + "\n";
      case H_ENCAP:
	return "IP\n";
      case H_FILENAME:
	return fd->_filename + "\n";
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
FromTcpdump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromTcpdump *fd = static_cast<FromTcpdump *>(e);
    String s = cp_uncomment(s_in);
    switch ((intptr_t)thunk) {
      case H_ACTIVE: {
	  bool active;
	  if (cp_bool(s, &active)) {
	      fd->_active = active;
	      if (fd->output_is_push(0) && active && !fd->_task.scheduled())
		  fd->_task.reschedule();
	      else if (!fd->output_is_push(0))
		  fd->_notifier.set_listeners(active);
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
FromTcpdump::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, (void *)H_SAMPLING_PROB);
    add_read_handler("active", read_handler, (void *)H_ACTIVE);
    add_write_handler("active", write_handler, (void *)H_ACTIVE);
    add_read_handler("encap", read_handler, (void *)H_ENCAP);
    add_read_handler("filename", read_handler, (void *)H_FILENAME);
    add_read_handler("filesize", read_handler, (void *)H_FILESIZE);
    add_read_handler("filepos", read_handler, (void *)H_FILEPOS);
    add_write_handler("stop", write_handler, (void *)H_STOP);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel IPSummaryDumpInfo)
EXPORT_ELEMENT(FromTcpdump)
#include <click/bighashmap.cc>
CLICK_ENDDECLS
