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
#include <click/straccum.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <click/packet_anno.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
CLICK_DECLS

#ifdef i386
# define GET4(p)	ntohl(*reinterpret_cast<const uint32_t *>((p)))
# define GET2(p)	ntohs(*reinterpret_cast<const uint16_t *>((p)))
#else
# define GET4(p)	(((p)[0]<<24) | ((p)[1]<<16) | ((p)[2]<<8) | (p)[3])
# define GET2(p)	(((p)[0]<<8) | (p)[1])
#endif
#define GET1(p)		((p)[0])

FromIPSummaryDump::FromIPSummaryDump()
    : Element(0, 1), _fd(-1), _buffer(0), _pos(0), _len(0), _buffer_len(0),
      _work_packet(0), _task(this), _pipe(0)
{
    MOD_INC_USE_COUNT;
}

FromIPSummaryDump::~FromIPSummaryDump()
{
    MOD_DEC_USE_COUNT;
}

void *
FromIPSummaryDump::cast(const char *n)
{
    if (strcmp(n, "Notifier") == 0 && !output_is_push(0)) {
	_notifier.initialize(router());
	return &_notifier;
    } else
	return Element::cast(n);
}

int
FromIPSummaryDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true, zero = false, multipacket = false;
    uint8_t default_proto = IP_PROTO_TCP;
    _sampling_prob = (1 << SAMPLING_SHIFT);
    String default_contents, default_flowid;
    
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
		    "DEFAULT_FLOWID", cpArgument, "default flow ID", &default_flowid,
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
    _have_flowid = _have_aggregate = _use_flowid = _use_aggregate = _binary = false;
    if (default_contents)
	bang_data(default_contents, errh);
    if (default_flowid)
	bang_flowid(default_flowid, 0, errh);
    return 0;
}

int
FromIPSummaryDump::error_helper(ErrorHandler *errh, const char *x)
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    return errh->error("%s:%s%d: %s", _filename.cc(), (_binary ? "record " : ""), _recordno, x);
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

    if (_binary) {
	if (_pos > _len - 4) {
	    int errcode = read_buffer(errh);
	    if (errcode <= 0)
		return errcode;
	}
	int record_length = GET4(_buffer + _pos) & 0x7FFFFFFFU;
	if (record_length == 0)
	    return error_helper(errh, "zero-length binary record");
	while (_pos > _len - record_length) {
	    int errcode = read_buffer(errh);
	    if (errcode <= 0)
		return errcode;
	}
	if (_buffer[_pos] & 0x80) { // textual interpolation
	    int l = record_length;
	    while (l > 0 && _buffer[_pos + l - 1] == 0)
		l--;
	    result = String::stable_string(_buffer + _pos + 4, l - 4);
	} else {
	    _buffer[_pos] = 0;	// ensure it's not '!' or '#'
	    result = String::stable_string(_buffer + _pos, record_length);
	}
	_pos += record_length;
	_recordno++;
	return 1;
    }
    
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
	    _recordno++;
	    return 1;
	}
    }
}

int
FromIPSummaryDump::initialize(ErrorHandler *errh)
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

    _pos = _len = _file_offset = _save_char = _recordno = 0;
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

    _minor_version = MINOR_VERSION; // expected minor version
    String line;
    if (read_line(line, errh) < 0)
	return -1;
    else if (line.substring(0, 14) == "!IPSummaryDump") {
	int major_version;
	if (sscanf(line.cc() + 14, " %d.%d", &major_version, &_minor_version) == 2) {
	    if (major_version != MAJOR_VERSION || _minor_version > MINOR_VERSION) {
		errh->warning("%s: unexpected IPSummaryDump version %d.%d", _filename.cc(), major_version, _minor_version);
		_minor_version = MINOR_VERSION;
	    }
	}
    } else {
	// parse line again, warn if this doesn't look like a dump
	if (line.substring(0, 8) != "!creator" && line.substring(0, 5) != "!data") {
	    if (!_contents.size() /* don't warn on DEFAULT_CONTENTS */)
		errh->warning("%s: missing banner line; is this an IP summary dump?", _filename.cc());
	}
	if (_save_char)
	    _buffer[_pos] = _save_char;
	_pos = _save_char = _recordno = 0;
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
	    error_helper(errh, ("warning: unknown content type `" + word + "'").c_str());
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

    // recheck whether to use `!flowid' and `!aggregate'
    check_defaults();
}

void
FromIPSummaryDump::check_defaults()
{
    _use_flowid = false;
    _flowid = (_have_flowid ? _given_flowid : IPFlowID());
    _use_aggregate = _have_aggregate;
    for (int i = 0; i < _contents.size(); i++)
	if (_contents[i] == W_SRC)
	    _flowid.set_saddr(IPAddress());
	else if (_contents[i] == W_DST)
	    _flowid.set_daddr(IPAddress());
	else if (_contents[i] == W_SPORT)
	    _flowid.set_sport(0);
	else if (_contents[i] == W_DPORT)
	    _flowid.set_dport(0);
	else if (_contents[i] == W_AGGREGATE)
	    _use_aggregate = false;
    if (_flowid || _flowid.sport() || _flowid.dport())
	_use_flowid = true;
}

void
FromIPSummaryDump::bang_flowid(const String &line, click_ip *iph,
			       ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(line, words);

    IPAddress src, dst;
    uint32_t sport = 0, dport = 0, proto = 0;
    if (words.size() < 5
	|| (!cp_ip_address(words[1], &src) && words[1] != "-")
	|| (!cp_unsigned(words[2], &sport) && words[2] != "-")
	|| (!cp_ip_address(words[3], &dst) && words[3] != "-")
	|| (!cp_unsigned(words[4], &dport) && words[4] != "-")
	|| sport > 65535 || dport > 65535) {
	error_helper(errh, "bad !flowid specification");
	_have_flowid = _use_flowid = false;
    } else {
	if (words.size() >= 6) {
	    if (cp_unsigned(words[5], &proto) && proto < 256)
		_default_proto = proto;
	    else if (words[5] == "T")
		_default_proto = IP_PROTO_TCP;
	    else if (words[5] == "U")
		_default_proto = IP_PROTO_UDP;
	    else if (words[5] == "I")
		_default_proto = IP_PROTO_ICMP;
	    else
		error_helper(errh, "bad protocol in !flowid");
	}
	_given_flowid = IPFlowID(src, htons(sport), dst, htons(dport));
	_have_flowid = true;
	check_defaults();
	if (iph)
	    iph->ip_p = _default_proto;
    }
}

void
FromIPSummaryDump::bang_aggregate(const String &line, ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(line, words);

    if (words.size() != 2
	|| !cp_unsigned(words[1], &_aggregate)) {
	error_helper(errh, "bad !aggregate specification");
	_have_aggregate = _use_aggregate = false;
    } else {
	_have_aggregate = true;
	check_defaults();
    }
}

void
FromIPSummaryDump::bang_binary(const String &line, ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(line, words);
    if (words.size() != 1)
	error_helper(errh, "bad !binary specification");
    if (_save_char)
	_buffer[_pos] = _save_char;
    for (int i = 0; i < _contents.size(); i++)
	if (content_binary_size(_contents[i]) < 0) {
	    error_helper(errh, "contents incompatible with !binary");
	    _pos = 0xFFFFFFFFU;	// prevent reading more data
	}
    _binary = true;
}

static void
append_net_uint32_t(StringAccum &sa, uint32_t u)
{
    sa << (char)(u >> 24) << (char)(u >> 16) << (char)(u >> 8) << (char)u;
}

int
FromIPSummaryDump::parse_ip_opt_ascii(const char *data, int pos, String *result, int contents)
{
    StringAccum sa;
    int original_pos = pos;
    
    while (1) {
	char *next;
	uint32_t u1;

	if (strncmp(data + pos, "rr{", 3) == 0
	    && (contents & DO_IPOPT_ROUTE)) {
	    // record route
	    sa << (char)IPOPT_RR;
	    pos += 3;
	  parse_route:
	    int sa_pos = sa.length() - 1;
	    int pointer = -1;
	    sa << '\0' << '\0';
	    next = const_cast<char *>(data + pos);
	    // loop over entries
	    while (1) {
		if (*next == '^' && pointer < 0)
		    pointer = sa.length() - sa_pos + 1, next++;
		if (!isdigit(*next))
		    break;
		for (int i = 0; i < 4; i++) {
		    if (!isdigit(*next)
			|| (u1 = strtoul(next, &next, 0)) > 255
			|| (i < 3 && *next++ != '.'))
			goto bad_opt;
		    sa << (char)u1;
		}
		if (*next == ',')
		    next++;
	    }
	    if (*next != '}')	// must end with a brace
		goto bad_opt;
	    sa[sa_pos + 2] = (pointer >= 0 ? pointer : sa.length() - sa_pos + 1);
	    if (next[1] == '+' && isdigit(next[2])
		&& (u1 = strtoul(next + 2, &next, 0)) < 64)
		sa.append_fill('\0', u1 * 4);
	    else
		next++;
	    if (sa.length() - sa_pos > 255)
		goto bad_opt;
	    sa[sa_pos + 1] = sa.length() - sa_pos;
	    pos = next - data;
	    
	} else if (strncmp(data + pos, "ssrr{", 5) == 0
		   && (contents & DO_IPOPT_ROUTE)) {
	    // strict source route option
	    sa << (char)IPOPT_SSRR;
	    pos += 5;
	    goto parse_route;
	    
	} else if (strncmp(data + pos, "lsrr{", 5) == 0
		   && (contents & DO_IPOPT_ROUTE)) {
	    // loose source route option
	    sa << (char)IPOPT_LSRR;
	    pos += 5;
	    goto parse_route;
	    
	} else if ((strncmp(data + pos, "ts{", 3) == 0
		    || strncmp(data + pos, "ts.", 3) == 0)
		   && (contents & DO_IPOPT_TS)) {
	    // timestamp option
	    int sa_pos = sa.length();
	    sa << (char)IPOPT_TS << (char)0 << (char)0 << (char)0;
	    uint32_t top_bit;
	    int flag = -1;
	    if (data[pos + 2] == '.') {
		if (strncmp(data + pos + 2, ".ip{", 4) == 0)
		    flag = 1, pos += 6;
		else if (strncmp(data + pos + 2, ".preip{", 7) == 0)
		    flag = 3, pos += 9;
		else if (isdigit(data[pos + 3])
			 && (flag = strtoul(data + pos + 3, &next, 0)) <= 15
			 && *next == '{')
		    pos = (next + 1) - data;
		else
		    goto bad_opt;
	    } else
		pos += 3;
	    next = const_cast<char *>(data + pos);
	    int pointer = -1;
	    
	    // loop over timestamp entries
	    while (1) {
		if (*next == '^' && pointer < 0)
		    pointer = sa.length() - sa_pos + 1, next++;
		if (!isdigit(*next) && *next != '!')
		    break;
		char *entry = next;
	      retry_entry:
		if (flag == 1 || flag == 3 || flag == -2) {
		    // parse IP address
		    for (int i = 0; i < 4; i++) {
			if (!isdigit(*next)
			    || (u1 = strtoul(next, &next, 0)) > 255
			    || (i < 3 && *next++ != '.'))
			    goto bad_opt;
			sa << (char)u1;
		    }
		    // prespecified IPs if we get here
		    if (pointer >= 0 && flag == -2)
			flag = 3;
		    // check for valid value
		    if (next[0] == '=' && (isdigit(next[1]) || next[1] == '!'))
			next++;
		    else if ((next[0] != '=' || next[1] == '?') && pointer < 0)
			goto bad_opt;
		    else {
			next += (next[0] == '=' ? 2 : 0);
			sa << (char)0 << (char)0 << (char)0 << (char)0;
			goto done_entry;
		    }
		}
		
		// parse timestamp value
		top_bit = 0;
		if (next[0] == '!')
		    top_bit = 0x80000000U, next++;
		if (!isdigit(*next))
		    goto bad_opt;
		u1 = strtoul(next, &next, 0);
		if (*next == '.' && flag == -1) {
		    flag = -2;
		    next = entry;
		    goto retry_entry;
		} else if (flag == -1)
		    flag = 0;
		u1 |= top_bit;
		sa << (char)(u1 >> 24) << (char)(u1 >> 16) << (char)(u1 >> 8) << (char)u1;
	      done_entry:
		// check separator
		if (*next == ',')
		    next++;
	    }
	    
	    // done with entries
	    if (*next++ != '}')
		goto bad_opt;
	    if (flag == -2)
		flag = 1;
	    sa[sa_pos + 2] = (pointer >= 0 ? pointer : sa.length() - sa_pos + 1);
	    if (next[0] == '+' && isdigit(next[1])
		&& (u1 = strtoul(next + 1, &next, 0)) < 64)
		sa.append_fill('\0', u1 * (flag == 1 || flag == 3 ? 8 : 4));
	    int overflow = 0;
	    if (next[0] == '+' && next[1] == '+' && isdigit(next[2])
		&& (u1 = strtoul(next + 2, &next, 0)) < 16)
		overflow = u1;
	    sa[sa_pos + 3] = (overflow << 4) | flag;
	    if (sa.length() - sa_pos > 255)
		goto bad_opt;
	    sa[sa_pos + 1] = sa.length() - sa_pos;
	    pos = next - data;
	    
	} else if (isdigit(data[pos])
		   && (contents & DO_IPOPT_UNKNOWN)) {
	    // unknown option
	    u1 = strtoul(data + pos, &next, 0);
	    if (u1 >= 256)
		goto bad_opt;
	    sa << (char)u1;
	    if (*next == '=' && isdigit(next[1])) {
		int pos0 = sa.length();
		sa << (char)0;
		do {
		    u1 = strtoul(next + 1, &next, 0);
		    if (u1 >= 256)
			goto bad_opt;
		    sa << (char)u1;
		} while (*next == ':' && isdigit(next[1]));
		if (sa.length() > pos0 + 254)
		    goto bad_opt;
		sa[pos0] = (char)(sa.length() - pos0 + 1);
	    }
	    pos = next - data;
	} else if (strncmp(data + pos, "nop", 3) == 0
		   && (contents & DO_IPOPT_PADDING)) {
	    sa << (char)IPOPT_NOP;
	    pos += 3;
	} else if (strncmp(data + pos, "eol", 3) == 0
		   && (contents & DO_IPOPT_PADDING)
		   && data[pos + 3] != ',') {
	    sa << (char)IPOPT_EOL;
	    pos += 3;
	} else
	    goto bad_opt;

	if (isspace(data[pos]) || !data[pos]) {
	    // check for improper padding
	    while (sa.length() > 40 && sa[0] == TCPOPT_NOP) {
		memmove(&sa[0], &sa[1], sa.length() - 1);
		sa.pop_back();
	    }
	    // options too long?
	    if (sa.length() > 40)
		goto bad_opt;
	    // otherwise ok
	    *result = sa.take_string();
	    return pos;
	} else if (data[pos] != ',' && data[pos] != ';')
	    goto bad_opt;

	pos++;
    }

  bad_opt:
    *result = String();
    return original_pos;
}

int
FromIPSummaryDump::parse_tcp_opt_ascii(const char *data, int pos, String *result, int contents)
{
    StringAccum sa;
    int original_pos = pos;
    
    while (1) {
	char *next;
	uint32_t u1, u2;

	if (strncmp(data + pos, "mss", 3) == 0
	    && (contents & DO_TCPOPT_MSS)) {
	    u1 = strtoul(data + pos + 3, &next, 0);
	    if (u1 <= 0xFFFFU)
		sa << (char)TCPOPT_MAXSEG << (char)TCPOLEN_MAXSEG << (char)(u1 >> 8) << (char)u1;
	    else
		goto bad_opt;
	    pos = next - data;
	} else if (strncmp(data + pos, "wscale", 6) == 0
		   && (contents & DO_TCPOPT_WSCALE)) {
	    u1 = strtoul(data + pos + 6, &next, 0);
	    if (u1 <= 255)
		sa << (char)TCPOPT_WSCALE << (char)TCPOLEN_WSCALE << (char)u1;
	    else
		goto bad_opt;
	    pos = next - data;
	} else if (strncmp(data + pos, "sackok", 6) == 0
		   && (contents & DO_TCPOPT_SACK)) {
	    sa << (char)TCPOPT_SACK_PERMITTED << (char)TCPOLEN_SACK_PERMITTED;
	    pos += 6;
	} else if (strncmp(data + pos, "sack", 4) == 0
		   && (contents & DO_TCPOPT_SACK)) {
	    // combine adjacent SACK options into a block
	    int sa_pos = sa.length();
	    sa << (char)TCPOPT_SACK << (char)0;
	    pos += 4;
	    while (1) {
		u1 = strtoul(data + pos, &next, 0);
		if (*next != ':')
		    goto bad_opt;
		u2 = strtoul(next + 1, &next, 0);
		append_net_uint32_t(sa, u1);
		append_net_uint32_t(sa, u2);
		pos = next - data;
		if (next < data + pos + 3 // at least 1 digit in each block
		    || strncmp(next, ",sack", 5) != 0
		    || !isdigit(next[5]))
		    break;
		pos = (next + 5) - data;
	    }
	    sa[sa_pos + 1] = (char)(sa.length() - sa_pos);
	} else if (strncmp(data + pos, "ts", 2) == 0
		   && (contents & DO_TCPOPT_TIMESTAMP)) {
	    u1 = strtoul(data + pos + 2, &next, 0);
	    if (*next != ':')
		goto bad_opt;
	    u2 = strtoul(next + 1, &next, 0);
	    if (sa.length() == 0)
		sa << (char)TCPOPT_NOP << (char)TCPOPT_NOP;
	    sa << (char)TCPOPT_TIMESTAMP << (char)TCPOLEN_TIMESTAMP;
	    append_net_uint32_t(sa, u1);
	    append_net_uint32_t(sa, u2);
	    pos = next - data;
	} else if (isdigit(data[pos])
		   && (contents & DO_TCPOPT_UNKNOWN)) {
	    u1 = strtoul(data + pos, &next, 0);
	    if (u1 >= 256)
		goto bad_opt;
	    sa << (char)u1;
	    if (*next == '=' && isdigit(next[1])) {
		int pos0 = sa.length();
		sa << (char)0;
		do {
		    u1 = strtoul(next + 1, &next, 0);
		    if (u1 >= 256)
			goto bad_opt;
		    sa << (char)u1;
		} while (*next == ':' && isdigit(next[1]));
		if (sa.length() > pos0 + 254)
		    goto bad_opt;
		sa[pos0] = (char)(sa.length() - pos0 + 1);
	    }
	    pos = next - data;
	} else if (strncmp(data + pos, "nop", 3) == 0
		   && (contents & DO_TCPOPT_PADDING)) {
	    sa << (char)TCPOPT_NOP;
	    pos += 3;
	} else if (strncmp(data + pos, "eol", 3) == 0
		   && (contents & DO_TCPOPT_PADDING)
		   && data[pos + 3] != ',') {
	    sa << (char)TCPOPT_EOL;
	    pos += 3;
	} else
	    goto bad_opt;

	if (isspace(data[pos]) || !data[pos]) {
	    // check for improper padding
	    while (sa.length() > 40 && sa[0] == TCPOPT_NOP) {
		memmove(&sa[0], &sa[1], sa.length() - 1);
		sa.pop_back();
	    }
	    // options too long?
	    if (sa.length() > 40)
		goto bad_opt;
	    // otherwise ok
	    *result = sa.take_string();
	    return pos;
	} else if (data[pos] != ',' && data[pos] != ';')
	    goto bad_opt;

	pos++;
    }

  bad_opt:
    *result = String();
    return original_pos;
}

static WritablePacket *
handle_ip_opt(WritablePacket *q, const String &optstr)
{
    int olen = (optstr.length() + 3) & ~3;
    if (!(q = q->put(olen)))
	return 0;
    memmove(q->transport_header() + olen, q->transport_header(), sizeof(click_tcp));
    q->ip_header()->ip_hl = (sizeof(click_ip) + olen) >> 2;
    memcpy(q->ip_header() + 1, optstr.data(), optstr.length());
    if (optstr.length() & 3)
	*(reinterpret_cast<uint8_t *>(q->ip_header() + 1) + optstr.length()) = IPOPT_EOL;
    q->set_ip_header(q->ip_header(), sizeof(click_ip) + olen);
    return q;
}

static WritablePacket *
handle_tcp_opt(WritablePacket *q, const String &optstr)
{
    int olen = (optstr.length() + 3) & ~3;
    if (!(q = q->put(olen)))
	return 0;
    q->tcp_header()->th_off = (sizeof(click_tcp) + olen) >> 2;
    memcpy(q->tcp_header() + 1, optstr.data(), optstr.length());
    if (optstr.length() & 3)
	*(reinterpret_cast<uint8_t *>(q->tcp_header() + 1) + optstr.length()) = TCPOPT_EOL;
    return q;
}

Packet *
FromIPSummaryDump::read_packet(ErrorHandler *errh)
{
    WritablePacket *q = Packet::make(0, (const unsigned char *)0, sizeof(click_ip) + sizeof(click_tcp), 8);
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
    StringAccum payload;
    String ip_opt;
    String tcp_opt;
    
    while (1) {

	if (read_line(line, errh) <= 0) {
	    q->kill();
	    return 0;
	}

	const char *data = line.data();
	int len = line.length();

	if (len == 0)
	    continue;
	else if (data[0] == '!') {
	    if (len >= 6 && memcmp(data, "!data", 5) == 0 && isspace(data[5]))
		bang_data(line, errh);
	    else if (len >= 8 && memcmp(data, "!flowid", 7) == 0 && isspace(data[7]))
		bang_flowid(line, iph, errh);
	    else if (len >= 11 && memcmp(data, "!aggregate", 10) == 0 && isspace(data[10]))
		bang_aggregate(line, errh);
	    else if (len >= 8 && memcmp(data, "!binary", 7) == 0 && isspace(data[7]))
		bang_binary(line, errh);
	    continue;
	} else if (data[0] == '#')
	    continue;

	int ok = (_binary ? 1 : 0);
	int ip_ok = 0;
	int pos = (_binary ? 4 : 0);
	uint32_t byte_count = 0;
	uint32_t payload_len = 0;
	bool have_payload_len = false;
	bool have_payload = false;
	bool have_ip_opt = false;
	bool have_tcp_opt = false;
	
	for (int i = 0; pos < len && i < _contents.size(); i++) {
	    int original_pos = pos;
	    char *next;
	    uint32_t u1 = 0, u2 = 0;

	    // check binary case
	    if (_binary) {
		switch (_contents[i]) {
		  case W_TIMESTAMP:
		  case W_FIRST_TIMESTAMP:
		    u1 = GET4(data + pos);
		    u2 = GET4(data + pos + 4);
		    pos += 8;
		    break;
		  case W_TIMESTAMP_SEC:
		  case W_TIMESTAMP_USEC:
		  case W_LENGTH:
		  case W_PAYLOAD_LENGTH:
		  case W_TCP_SEQ:
		  case W_TCP_ACK:
		  case W_COUNT:
		  case W_AGGREGATE:
		  case W_SRC:
		  case W_DST:
		    u1 = GET4(data + pos);
		    pos += 4;
		    break;
		  case W_IPID:
		  case W_SPORT:
		  case W_DPORT:
		  case W_FRAGOFF:
		  case W_TCP_WINDOW:
		    u1 = GET2(data + pos);
		    pos += 2;
		    break;
		  case W_PROTO:
		  case W_TCP_FLAGS:
		  case W_LINK:
		    u1 = GET1(data + pos);
		    pos++;
		    break;
		  case W_FRAG:
		    // XXX less checking here
		    if (data[pos] == 'F')
			u1 = htons(IP_MF);
		    else if (data[pos] == 'f')
			u1 = htons(100); // random number
		    pos++;	// u1 already 0
		    break;
		  case W_IP_OPT:
		    if (pos + 1 + data[pos] <= len) {
			ip_opt = line.substring(pos + 1, data[pos]);
			have_ip_opt = true;
			pos += data[pos] + 1;
		    }
		    break;
		  case W_TCP_OPT:
		  case W_TCP_NTOPT:
		  case W_TCP_SACK:
		    if (pos + 1 + data[pos] <= len) {
			tcp_opt = line.substring(pos + 1, data[pos]);
			have_tcp_opt = true;
			pos += data[pos] + 1;
		    }
		    break;
		}
		goto store_contents;
	    }

	    // otherwise, ascii
	    // first, parse contents
	    switch (_contents[i]) {

	      case W_TIMESTAMP:
	      case W_FIRST_TIMESTAMP:
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
	      case W_AGGREGATE:
	      case W_TCP_WINDOW:
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
		    u1 = htons(100);	// random number
		    pos++;
		} else if (data[pos] == '.')
		    pos++;	// u1 already 0
		break;

	      case W_FRAGOFF:
		u1 = strtoul(data + pos, &next, 0);
		if (_minor_version == 0) // old-style file
		    u1 <<= 3;
		if (next > data + pos && (u1 & 7) == 0 && u1 < 65536) {
		    u1 >>= 3;
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
		    while (tcp_flag_mapping[data[pos]]) {
			u1 |= 1 << (tcp_flag_mapping[data[pos]] - 1);
			pos++;
		    }
		break;

	      case W_IP_OPT:
		if (data[pos] == '.')
		    pos++;
		else if (data[pos] != '-') {
		    have_ip_opt = true;
		    pos = parse_ip_opt_ascii(data, pos, &ip_opt, DO_IPOPT_ALL);
		}
		break;

	      case W_TCP_SACK:
		if (data[pos] == '.')
		    pos++;
		else if (data[pos] != '-') {
		    have_tcp_opt = true;
		    pos = parse_tcp_opt_ascii(data, pos, &tcp_opt, DO_TCPOPT_SACK);
		}
		break;
		
	      case W_TCP_NTOPT:
		if (data[pos] == '.')
		    pos++;
		else if (data[pos] != '-') {
		    have_tcp_opt = true;
		    pos = parse_tcp_opt_ascii(data, pos, &tcp_opt, DO_TCPOPT_NTALL);
		}
		break;
		
	      case W_TCP_OPT:
		if (data[pos] == '.')
		    pos++;
		else if (data[pos] != '-') {
		    have_tcp_opt = true;
		    pos = parse_tcp_opt_ascii(data, pos, &tcp_opt, DO_TCPOPT_ALL);
		}
		break;
		
	      case W_LINK:
		if (data[pos] == '>' || data[pos] == 'L') {
		    u1 = 0;
		    pos++;
		} else if (data[pos] == '<' || data[pos] == 'X' || data[pos] == 'R') {
		    u1 = 1;
		    pos++;
		} else {
		    u1 = strtoul(data + pos, &next, 0);
		    pos = next - data;
		}
		break;

	      case W_PAYLOAD:
		if (data[pos] == '\"') {
		    payload.clear();
		    int fpos = pos + 1;
		    for (pos++; pos < len && data[pos] != '\"'; pos++)
			if (data[pos] == '\\' && pos < len - 1) {
			    payload.append(data + fpos, pos - fpos);
			    fpos = cp_process_backslash(data, pos, len, payload);
			    pos = fpos - 1; // account for loop increment
			}
		    payload.append(data + fpos, pos - fpos);
		    // bag payload if it didn't parse correctly
		    if (pos >= len || data[pos] != '\"')
			pos = original_pos;
		    else {
			have_payload = have_payload_len = true;
			payload_len = payload.length();
		    }
		}
		break;
		
	    }

	    // check whether we correctly parsed something
	    {
		bool this_ok = (pos > original_pos && (!data[pos] || isspace(data[pos])));
		while (data[pos] && !isspace(data[pos]))
		    pos++;
		while (isspace(data[pos]))
		    pos++;
		if (!this_ok)
		    continue;
	    }

	    // store contents
	  store_contents:
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
		iph->ip_src.s_addr = htonl(u1), ip_ok++;
		break;

	      case W_DST:
		iph->ip_dst.s_addr = htonl(u1), ip_ok++;
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
		    iph->ip_p = u1, ip_ok++;
		break;

	      case W_IPID:
		if (u1 <= 0xFFFF)
		    iph->ip_id = htons(u1), ip_ok++;
		break;

	      case W_FRAG:
		iph->ip_off = u1, ip_ok++;
		break;

	      case W_FRAGOFF:
		if ((u1 & ~IP_MF) <= IP_OFFMASK)
		    iph->ip_off = htons(u1), ip_ok++;
		break;

	      case W_SPORT:
		if (u1 <= 0xFFFF)
		    q->udp_header()->uh_sport = htons(u1), ip_ok++;
		break;

	      case W_DPORT:
		if (u1 <= 0xFFFF)
		    q->udp_header()->uh_dport = htons(u1), ip_ok++;
		break;

	      case W_TCP_SEQ:
		q->tcp_header()->th_seq = htonl(u1), ip_ok++;
		break;

	      case W_TCP_ACK:
		q->tcp_header()->th_ack = htonl(u1), ip_ok++;
		break;

	      case W_TCP_FLAGS:
		if (u1 <= 0xFF)
		    q->tcp_header()->th_flags = u1, ip_ok++;
		break;

	      case W_TCP_WINDOW:
		if (u1 <= 0xFFFF)
		    q->tcp_header()->th_win = htons(u1), ip_ok++;
		break;
		
	      case W_COUNT:
		if (u1)
		    SET_EXTRA_PACKETS_ANNO(q, u1 - 1), ok++;
		break;

	      case W_LINK:
		SET_PAINT_ANNO(q, u1), ok++;
		break;

	      case W_AGGREGATE:
		SET_AGGREGATE_ANNO(q, u1), ok++;
		break;

	      case W_FIRST_TIMESTAMP:
		if (u2 < 1000000) {
		    struct timeval tv;
		    tv.tv_sec = u1; tv.tv_usec = u2;
		    SET_FIRST_TIMESTAMP_ANNO(q, tv);
		    ok++;
		}
		break;

	    }
	}

	if (!ok && !ip_ok)
	    break;

	// append IP options if any
	if (have_ip_opt && ip_opt) {
	    if (!(q = handle_ip_opt(q, ip_opt)))
		return 0;
	    else		// iph may have changed!! (don't use tcph etc.)
		iph = q->ip_header();
	}
	
	// set TCP offset to a reasonable value; possibly reduce packet length
	if (iph->ip_p == IP_PROTO_TCP && IP_FIRSTFRAG(iph)) {
	    if (!have_tcp_opt || !tcp_opt)
		q->tcp_header()->th_off = sizeof(click_tcp) >> 2;
	    else if (!(q = handle_tcp_opt(q, tcp_opt)))
		return 0;
	    else		// iph may have changed!!
		iph = q->ip_header();
	} else if (iph->ip_p == IP_PROTO_UDP && IP_FIRSTFRAG(iph))
	    q->take(sizeof(click_tcp) - sizeof(click_udp));
	else
	    q->take(sizeof(click_tcp));
	
	if (have_payload) {	// XXX what if byte_count indicates IP options?
	    int old_length = q->length();
	    iph->ip_len = ntohs(old_length + payload.length());
	    if ((q = q->put(payload.length())))
		memcpy(q->data() + old_length, payload.data(), payload.length());
	} else if (byte_count) {
	    iph->ip_len = ntohs(byte_count);
	    SET_EXTRA_LENGTH_ANNO(q, byte_count - q->length());
	} else if (have_payload_len) {
	    iph->ip_len = ntohs(q->length() + payload_len);
	    SET_EXTRA_LENGTH_ANNO(q, payload_len);
	} else
	    iph->ip_len = ntohs(q->length());

	// set data from flow ID
	if (_use_flowid) {
	    IPFlowID flowid = (PAINT_ANNO(q) ? _flowid.rev() : _flowid);
	    if (flowid.saddr())
		iph->ip_src = flowid.saddr();
	    if (flowid.daddr())
		iph->ip_dst = flowid.daddr();
	    if (flowid.sport() && IP_FIRSTFRAG(iph))
		q->tcp_header()->th_sport = flowid.sport();
	    if (flowid.dport() && IP_FIRSTFRAG(iph))
		q->tcp_header()->th_dport = flowid.dport();
	    if (_use_aggregate)
		SET_AGGREGATE_ANNO(q, _aggregate);
	} else if (!ip_ok)
	    q->set_network_header(0, 0);
	
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

bool
FromIPSummaryDump::run_task()
{
    if (!_active)
	return false;
    Packet *p;

    while (1) {
	p = (_work_packet ? _work_packet : read_packet(0));
	if (!p) {
	    if (_stop)
		router()->please_stop_driver();
	    return false;
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
    return true;
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
	    _notifier.set_listeners(false);
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

    _notifier.set_listeners(true);
    return p;
}


enum { H_SAMPLING_PROB, H_ACTIVE, H_ENCAP, H_FILENAME, H_FILESIZE, H_FILEPOS, H_STOP };

String
FromIPSummaryDump::read_handler(Element *e, void *thunk)
{
    FromIPSummaryDump *fd = static_cast<FromIPSummaryDump *>(e);
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
FromIPSummaryDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromIPSummaryDump *fd = static_cast<FromIPSummaryDump *>(e);
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
FromIPSummaryDump::add_handlers()
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
EXPORT_ELEMENT(FromIPSummaryDump)
CLICK_ENDDECLS
