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
    : _work_packet(0), _task(this)
{
    _ff.set_landmark_pattern("%f:%l");
}

FromIPSummaryDump::~FromIPSummaryDump()
{
}

void *
FromIPSummaryDump::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0 && !output_is_push(0)) {
	_notifier.initialize(router());
	return static_cast<Notifier *>(&_notifier);
    } else
	return Element::cast(n);
}

int
FromIPSummaryDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true, zero = true, checksum = false, multipacket = false;
    uint8_t default_proto = IP_PROTO_TCP;
    _sampling_prob = (1 << SAMPLING_SHIFT);
    String default_contents, default_flowid;
    
    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump file name", &_ff.filename(),
		    cpKeywords,
		    "STOP", cpBool, "stop driver when done?", &stop,
		    "ACTIVE", cpBool, "start active?", &active,
		    "ZERO", cpBool, "zero packet data?", &zero,
		    "CHECKSUM", cpBool, "set packet checksums?", &checksum,
		    "SAMPLE", cpUnsignedReal2, "sampling probability", SAMPLING_SHIFT, &_sampling_prob,
		    "PROTO", cpByte, "default IP protocol", &default_proto,
		    "MULTIPACKET", cpBool, "generate multiple packets per record?", &multipacket,
		    "DEFAULT_CONTENTS", cpArgument, "default contents of log", &default_contents,
		    "DEFAULT_FLOWID", cpArgument, "default flow ID", &default_flowid,
		    "CONTENTS", cpArgument, "default contents of log", &default_contents,
		    "FLOWID", cpArgument, "default flow ID", &default_flowid,
		    cpEnd) < 0)
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
    _checksum = checksum;
    _multipacket = multipacket;
    _have_flowid = _have_aggregate = _use_flowid = _use_aggregate = _binary = false;
    if (default_contents)
	bang_data(default_contents, errh);
    if (default_flowid)
	bang_flowid(default_flowid, 0, errh);
    return 0;
}

int
FromIPSummaryDump::read_binary(String &result, ErrorHandler *errh)
{
    assert(_binary);

    uint8_t record_storage[4];
    const uint8_t *record = _ff.get_unaligned(4, record_storage, errh);
    if (!record)
	return 0;
    int record_length = GET4(record) & 0x7FFFFFFFU;
    if (record_length < 4)
	return _ff.error(errh, "binary record too short");
    bool textual = (record[0] & 0x80 ? true : false);
    result = _ff.get_string(record_length - 4, errh);
    if (!result)
	return 0;
    if (textual) {
	const char *s = result.begin(), *e = result.end();
	while (e > s && e[-1] == 0)
	    e--;
	if (e != result.end())
	    result = result.substring(s, e);
    }
    _ff.set_lineno(_ff.lineno() + 1);
    return (textual ? 2 : 1);
}

int
FromIPSummaryDump::initialize(ErrorHandler *errh)
{
    // make sure notifier is initialized
    if (!output_is_push(0))
	_notifier.initialize(router());
    
    if (_ff.initialize(errh) < 0)
	return -1;
    
    _minor_version = IPSummaryDump::MINOR_VERSION; // expected minor version
    String line;
    if (_ff.peek_line(line, errh) < 0)
	return -1;
    else if (line.substring(0, 14) == "!IPSummaryDump") {
	int major_version;
	if (sscanf(line.c_str() + 14, " %d.%d", &major_version, &_minor_version) == 2) {
	    if (major_version != IPSummaryDump::MAJOR_VERSION || _minor_version > IPSummaryDump::MINOR_VERSION) {
		_ff.warning(errh, "unexpected IPSummaryDump version %d.%d", major_version, _minor_version);
		_minor_version = IPSummaryDump::MINOR_VERSION;
	    }
	}
	(void) _ff.read_line(line, errh); // throw away line
    } else {
	// parse line again, warn if this doesn't look like a dump
	if (line.substring(0, 8) != "!creator" && line.substring(0, 5) != "!data" && line.substring(0, 9) != "!contents") {
	    if (!_contents.size() /* don't warn on DEFAULT_CONTENTS */)
		_ff.warning(errh, "missing banner line; is this an IP summary dump?");
	}
    }
    
    _format_complaint = false;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return 0;
}

void
FromIPSummaryDump::cleanup(CleanupStage)
{
    _ff.cleanup();
    if (_work_packet)
	_work_packet->kill();
    _work_packet = 0;
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
	if (what >= W_NONE && what < W_LAST) {
	    _contents.push_back(what);
	    all_contents |= (1 << (what - W_NONE));
	} else if (i > 0 || (word != "!data" && word != "!contents")) {
	    _ff.warning(errh, "unknown content type '%s'", word.c_str());
	    _contents.push_back(W_NONE);
	}
    }

    if (_contents.size() == 0)
	_ff.error(errh, "no contents specified");

    // If we have W_IP_FRAGOFF, ignore W_IP_FRAG.
    if (all_contents & (1 << (W_IP_FRAGOFF - W_NONE)))
	for (int i = 0; i < _contents.size(); i++)
	    if (_contents[i] == W_IP_FRAG)
		_contents[i] = W_NONE;

    // recheck whether to use '!flowid' and '!aggregate'
    check_defaults();
}

void
FromIPSummaryDump::check_defaults()
{
    _use_flowid = false;
    _flowid = (_have_flowid ? _given_flowid : IPFlowID());
    _use_aggregate = _have_aggregate;
    for (int i = 0; i < _contents.size(); i++)
	if (_contents[i] == W_IP_SRC)
	    _flowid.set_saddr(IPAddress());
	else if (_contents[i] == W_IP_DST)
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
	_ff.error(errh, "bad !flowid specification");
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
		_ff.error(errh, "bad protocol in !flowid");
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
	_ff.error(errh, "bad !aggregate specification");
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
	_ff.error(errh, "bad !binary specification");
    for (int i = 0; i < _contents.size(); i++)
	if (content_binary_size(_contents[i]) < 0) {
	    _ff.error(errh, "contents incompatible with !binary");
	    // XXX _pos = 0xFFFFFFFFU;	// prevent reading more data
	}
    _binary = true;
    _ff.set_landmark_pattern("%f:record %l");
    _ff.set_lineno(1);
}

static void
append_net_uint32_t(StringAccum &sa, uint32_t u)
{
    sa << (char)(u >> 24) << (char)(u >> 16) << (char)(u >> 8) << (char)u;
}

const unsigned char *
FromIPSummaryDump::parse_ip_opt_ascii(const unsigned char *begin, const unsigned char *end, String *result, int contents)
{
    StringAccum sa;
    const unsigned char *s = begin;
    
    while (1) {
	const unsigned char *t;
	uint32_t u1;

	if (s + 3 < end && memcmp(s, "rr{", 3) == 0
	    && (contents & DO_IPOPT_ROUTE)) {
	    // record route
	    sa << (char)IPOPT_RR;
	    s += 3;
	  parse_route:
	    int sa_pos = sa.length() - 1;
	    int pointer = -1;
	    sa << '\0' << '\0';
	    // loop over entries
	    while (1) {
		if (s < end && *s == '^' && pointer < 0)
		    pointer = sa.length() - sa_pos + 1, s++;
		if (s >= end || !isdigit(*s))
		    break;
		for (int i = 0; i < 4; i++) {
		    u1 = 256;
		    s = cp_unsigned(s, end, 10, &u1) + (i < 3);
		    if (u1 > 255 || (i < 3 && (s > end || s[-1] != '.')))
			goto bad_opt;
		    sa << (char)u1;
		}
		if (s < end && *s == ',')
		    s++;
	    }
	    if (s >= end || *s != '}') // must end with a brace
		goto bad_opt;
	    sa[sa_pos + 2] = (pointer >= 0 ? pointer : sa.length() - sa_pos + 1);
	    if (s + 2 < end && s[1] == '+' && isdigit(s[2])) {
		s = cp_unsigned(s + 2, end, 10, &u1);
		if (u1 < 64)
		    sa.append_fill('\0', u1 * 4);
	    } else
		s++;
	    if (sa.length() - sa_pos > 255)
		goto bad_opt;
	    sa[sa_pos + 1] = sa.length() - sa_pos;
	    
	} else if (s + 5 < end && memcmp(s, "ssrr{", 5) == 0
		   && (contents & DO_IPOPT_ROUTE)) {
	    // strict source route option
	    sa << (char)IPOPT_SSRR;
	    s += 5;
	    goto parse_route;
	    
	} else if (s + 5 < end && memcmp(s, "lsrr{", 5) == 0
		   && (contents & DO_IPOPT_ROUTE)) {
	    // loose source route option
	    sa << (char)IPOPT_LSRR;
	    s += 5;
	    goto parse_route;
	    
	} else if (s + 3 < end
		   && (memcmp(s, "ts{", 3) == 0 || memcmp(s, "ts.", 3) == 0)
		   && (contents & DO_IPOPT_TS)) {
	    // timestamp option
	    int sa_pos = sa.length();
	    sa << (char)IPOPT_TS << (char)0 << (char)0 << (char)0;
	    uint32_t top_bit;
	    int flag = -1;
	    if (s[2] == '.') {
		if (s + 6 < end && memcmp(s + 3, "ip{", 3) == 0)
		    flag = 1, s += 6;
		else if (s + 9 < end && memcmp(s + 3, "preip{", 6) == 0)
		    flag = 3, s += 9;
		else if (isdigit(s[3])
			 && (t = cp_unsigned(s + 3, end, 0, (uint32_t *)&flag))
			 && flag <= 15 && t < end && *t == '{')
		    s = t + 1;
		else
		    goto bad_opt;
	    } else
		s += 3;
	    int pointer = -1;
	    
	    // loop over timestamp entries
	    while (1) {
		if (s < end && *s == '^' && pointer < 0)
		    pointer = sa.length() - sa_pos + 1, s++;
		if (s >= end || (!isdigit(*s) && *s != '!'))
		    break;
		const unsigned char *entry = s;
		
	      retry_entry:
		if (flag == 1 || flag == 3 || flag == -2) {
		    // parse IP address
		    for (int i = 0; i < 4; i++) {
			u1 = 256;
			s = cp_unsigned(s, end, 10, &u1) + (i < 3);
			if (u1 > 255 || (i < 3 && (s > end || s[-1] != '.')))
			    goto bad_opt;
			sa << (char)u1;
		    }
		    // prespecified IPs if we get here
		    if (pointer >= 0 && flag == -2)
			flag = 3;
		    // check for valid value: either "=[DIGIT]", "=!", "=?"
		    // (for pointer >= 0)
		    if (s + 1 < end && *s == '=') {
			if (isdigit(s[1]) || s[1] == '!')
			    s++;
			else if (s[1] == '?' && pointer >= 0) {
			    sa << (char)0 << (char)0 << (char)0 << (char)0;
			    s += 2;
			    goto done_entry;
			} else
			    goto bad_opt;
		    } else if (pointer >= 0) {
			sa << (char)0 << (char)0 << (char)0 << (char)0;
			goto done_entry;
		    } else
			goto bad_opt;
		}
		
		// parse timestamp value
		assert(s < end);
		top_bit = 0;
		if (*s == '!')
		    top_bit = 0x80000000U, s++;
		if (s >= end || !isdigit(*s))
		    goto bad_opt;
		s = cp_unsigned(s, end, 0, &u1);
		if (s < end && *s == '.' && flag == -1) {
		    flag = -2;
		    s = entry;
		    goto retry_entry;
		} else if (flag == -1)
		    flag = 0;
		u1 |= top_bit;
		append_net_uint32_t(sa, u1);
	      done_entry:
		// check separator
		if (s < end && *s == ',')
		    s++;
	    }
	    
	    // done with entries
	    if (s < end && *s++ != '}')
		goto bad_opt;
	    if (flag == -2)
		flag = 1;
	    sa[sa_pos + 2] = (pointer >= 0 ? pointer : sa.length() - sa_pos + 1);
	    if (s + 1 < end && *s == '+' && isdigit(s[1])
		&& (s = cp_unsigned(s + 1, end, 0, &u1))
		&& u1 < 64)
		sa.append_fill('\0', u1 * (flag == 1 || flag == 3 ? 8 : 4));
	    int overflow = 0;
	    if (s + 2 < end && *s == '+' && s[1] == '+' && isdigit(s[2])
		&& (s = cp_unsigned(s + 2, end, 0, &u1))
		&& u1 < 16)
		overflow = u1;
	    sa[sa_pos + 3] = (overflow << 4) | flag;
	    if (sa.length() - sa_pos > 255)
		goto bad_opt;
	    sa[sa_pos + 1] = sa.length() - sa_pos;
	    
	} else if (s < end && isdigit(*s) && (contents & DO_IPOPT_UNKNOWN)) {
	    // unknown option
	    s = cp_unsigned(s, end, 0, &u1);
	    if (u1 >= 256)
		goto bad_opt;
	    sa << (char)u1;
	    if (s + 1 < end && *s == '=' && isdigit(s[1])) {
		int pos0 = sa.length();
		sa << (char)0;
		do {
		    s = cp_unsigned(s + 1, end, 0, &u1);
		    if (u1 >= 256)
			goto bad_opt;
		    sa << (char)u1;
		} while (s + 1 < end && *s == ':' && isdigit(s[1]));
		if (sa.length() > pos0 + 254)
		    goto bad_opt;
		sa[pos0] = (char)(sa.length() - pos0 + 1);
	    }
	} else if (s + 3 <= end && memcmp(s, "nop", 3) == 0
		   && (contents & DO_IPOPT_PADDING)) {
	    sa << (char)IPOPT_NOP;
	    s += 3;
	} else if (s + 3 <= end && memcmp(s, "eol", 3) == 0
		   && (contents & DO_IPOPT_PADDING)
		   && (s + 3 == end || s[3] != ',')) {
	    sa << (char)IPOPT_EOL;
	    s += 3;
	} else
	    goto bad_opt;

	if (s >= end || isspace(*s)) {
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
	    return s;
	} else if (*s != ',' && *s != ';')
	    goto bad_opt;

	s++;
    }

  bad_opt:
    *result = String();
    return begin;
}

const unsigned char *
FromIPSummaryDump::parse_tcp_opt_ascii(const unsigned char *begin, const unsigned char *end, String *result, int contents)
{
    StringAccum sa;
    const unsigned char *s = begin;
    
    while (1) {
	uint32_t u1, u2;

	if (s + 3 < end && memcmp(s, "mss", 3) == 0
	    && (contents & DO_TCPOPT_MSS)) {
	    u1 = 0x10000U;	// bad value
	    s = cp_unsigned(s + 3, end, 0, &u1);
	    if (u1 <= 0xFFFFU)
		sa << (char)TCPOPT_MAXSEG << (char)TCPOLEN_MAXSEG << (char)(u1 >> 8) << (char)u1;
	    else
		goto bad_opt;
	} else if (s + 6 < end && memcmp(s, "wscale", 6) == 0
		   && (contents & DO_TCPOPT_WSCALE)) {
	    u1 = 256;		// bad value
	    s = cp_unsigned(s + 6, end, 0, &u1);
	    if (u1 <= 255)
		sa << (char)TCPOPT_WSCALE << (char)TCPOLEN_WSCALE << (char)u1;
	    else
		goto bad_opt;
	} else if (s + 6 <= end && memcmp(s, "sackok", 6) == 0
		   && (contents & DO_TCPOPT_SACK)) {
	    sa << (char)TCPOPT_SACK_PERMITTED << (char)TCPOLEN_SACK_PERMITTED;
	    s += 6;
	} else if (s + 4 < end && memcmp(s, "sack", 4) == 0
		   && (contents & DO_TCPOPT_SACK)) {
	    // combine adjacent SACK options into a block
	    int sa_pos = sa.length();
	    sa << (char)TCPOPT_SACK << (char)0;
	    s += 4;
	    while (1) {
		const unsigned char *t = cp_unsigned(s, end, 0, &u1);
		if (t >= end || (*t != ':' && *t != '-'))
		    goto bad_opt;
		t = cp_unsigned(t + 1, end, 0, &u2);
		append_net_uint32_t(sa, u1);
		append_net_uint32_t(sa, u2);
		if (t < s + 3) // at least 1 digit in each block
		    goto bad_opt;
		s = t;
		if (s + 5 >= end || memcmp(s, ",sack", 5) != 0)
		    break;
		s += 5;
	    }
	    sa[sa_pos + 1] = (char)(sa.length() - sa_pos);
	} else if (s + 2 < end && memcmp(s, "ts", 2) == 0
		   && (contents & DO_TCPOPT_TIMESTAMP)) {
	    const unsigned char *t = cp_unsigned(s + 2, end, 0, &u1);
	    if (t >= end || *t != ':')
		goto bad_opt;
	    t = cp_unsigned(t + 1, end, 0, &u2);
	    if (sa.length() == 0)
		sa << (char)TCPOPT_NOP << (char)TCPOPT_NOP;
	    sa << (char)TCPOPT_TIMESTAMP << (char)TCPOLEN_TIMESTAMP;
	    append_net_uint32_t(sa, u1);
	    append_net_uint32_t(sa, u2);
	    if (t < s + 5)	// at least 1 digit in each block
		goto bad_opt;
	    s = t;
	} else if (s < end && isdigit(*s)
		   && (contents & DO_TCPOPT_UNKNOWN)) {
	    s = cp_unsigned(s, end, 0, &u1);
	    if (u1 >= 256)
		goto bad_opt;
	    sa << (char)u1;
	    if (s + 1 < end && *s == '=' && isdigit(s[1])) {
		int pos0 = sa.length();
		sa << (char)0;
		do {
		    s = cp_unsigned(s + 1, end, 0, &u1);
		    if (u1 >= 256)
			goto bad_opt;
		    sa << (char)u1;
		} while (s + 1 < end && *s == ':' && isdigit(s[1]));
		if (sa.length() > pos0 + 254)
		    goto bad_opt;
		sa[pos0] = (char)(sa.length() - pos0 + 1);
	    }
	} else if (s + 3 <= end && memcmp(s, "nop", 3) == 0
		   && (contents & DO_TCPOPT_PADDING)) {
	    sa << (char)TCPOPT_NOP;
	    s += 3;
	} else if (s + 3 <= end && strncmp((const char *) s, "eol", 3) == 0
		   && (contents & DO_TCPOPT_PADDING)
		   && (s + 3 == end || s[3] != ',')) {
	    sa << (char)TCPOPT_EOL;
	    s += 3;
	} else
	    goto bad_opt;

	if (s >= end || isspace(*s)) {
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
	    return s;
	} else if (*s != ',' && *s != ';')
	    goto bad_opt;

	s++;
    }

  bad_opt:
    *result = String();
    return begin;
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
	tcph->th_sum = click_in_cksum_pseudohdr(csum, iph, q->transport_length());
    } else if (iph->ip_p == IP_PROTO_UDP) {
	click_udp *udph = q->udp_header();
	udph->uh_sum = 0;
	unsigned csum = click_in_cksum((uint8_t *)udph, q->transport_length());
	udph->uh_sum = click_in_cksum_pseudohdr(csum, iph, q->transport_length());
    }
}

Packet *
FromIPSummaryDump::read_packet(ErrorHandler *errh)
{
    WritablePacket *q = Packet::make(0, (const unsigned char *)0, sizeof(click_ip) + sizeof(click_tcp), 8);
    if (!q) {
	_ff.error(errh, strerror(ENOMEM));
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

	bool binary = _binary;

	if (binary) {
	    int result = read_binary(line, errh);
	    if (result <= 0) {
		q->kill();
		return 0;
	    } else
		binary = (result == 1);
	} else if (_ff.read_line(line, errh) <= 0) {
	    q->kill();
	    return 0;
	}

	const unsigned char *data = (const unsigned char *) line.begin();
	const unsigned char *end = (const unsigned char *) line.end();

	if (data == end)
	    continue;
	else if (!binary && data[0] == '!') {
	    if (data + 6 <= end && memcmp(data, "!data", 5) == 0 && isspace(data[5]))
		bang_data(line, errh);
	    else if (data + 8 <= end && memcmp(data, "!flowid", 7) == 0 && isspace(data[7]))
		bang_flowid(line, iph, errh);
	    else if (data + 11 <= end && memcmp(data, "!aggregate", 10) == 0 && isspace(data[10]))
		bang_aggregate(line, errh);
	    else if (data + 8 <= end && memcmp(data, "!binary", 7) == 0 && isspace(data[7]))
		bang_binary(line, errh);
	    else if (data + 10 <= end && memcmp(data, "!contents", 9) == 0 && isspace(data[9]))
		bang_data(line, errh);
	    continue;
	} else if (!binary && data[0] == '#')
	    continue;

	int ok = (binary ? 1 : 0);
	int ip_ok = 0;
	uint32_t byte_count = 0;
	uint32_t payload_len = 0;
	bool have_payload_len = false;
	bool have_payload = false;
	bool have_ip_opt = false;
	bool have_tcp_opt = false;
	
	for (int i = 0; data < end && i < _contents.size(); i++) {
	    const unsigned char *original_data = data;
	    const unsigned char *next;
	    uint32_t u1 = 0, u2 = 0;

	    // check binary case
	    if (binary) {
		switch (_contents[i]) {
		  case W_NONE:
		    break;
		  case W_TIMESTAMP:
		  case W_FIRST_TIMESTAMP:
		    u1 = GET4(data);
		    u2 = GET4(data + 4) * 1000;
		    data += 8;
		    break;
		  case W_NTIMESTAMP:
		  case W_FIRST_NTIMESTAMP:
		    u1 = GET4(data);
		    u2 = GET4(data + 4);
		    data += 8;
		    break;
		  case W_TIMESTAMP_USEC1:
		    u1 = GET4(data);
		    u2 = GET4(data + 4);
		    data += 8;
		    break;
		  case W_TIMESTAMP_SEC:
		  case W_TIMESTAMP_USEC:
		  case W_IP_LEN:
		  case W_PAYLOAD_LEN:
		  case W_IP_CAPTURE_LEN:
		  case W_TCP_SEQ:
		  case W_TCP_ACK:
		  case W_COUNT:
		  case W_AGGREGATE:
		  case W_IP_SRC:
		  case W_IP_DST:
		    u1 = GET4(data);
		    data += 4;
		    break;
		  case W_IP_ID:
		  case W_SPORT:
		  case W_DPORT:
		  case W_IP_FRAGOFF:
		  case W_TCP_WINDOW:
		  case W_TCP_URP:
		    u1 = GET2(data);
		    data += 2;
		    break;
		  case W_IP_PROTO:
		  case W_TCP_FLAGS:
		  case W_LINK:
		  case W_IP_TOS:
		  case W_IP_TTL:
		    u1 = GET1(data);
		    data++;
		    break;
		  case W_IP_FRAG:
		    // XXX less checking here
		    if (*data == 'F')
			u1 = htons(IP_MF);
		    else if (*data == 'f')
			u1 = htons(100); // random number
		    data++;	// u1 already 0
		    break;
		  case W_IP_OPT: {
		      const unsigned char *endopt = data + 1 + *data;
		      if (endopt <= end) {
			  ip_opt = line.substring((const char *) data + 1, (const char *) endopt);
			  have_ip_opt = true;
			  data = endopt;
		      }
		      break;
		  }
		  case W_TCP_OPT:
		  case W_TCP_NTOPT:
		  case W_TCP_SACK: {
		      const unsigned char *endopt = data + 1 + *data;
		      if (endopt <= end) {
			  tcp_opt = line.substring((const char *) data + 1, (const char *) endopt);
			  have_tcp_opt = true;
			  data = endopt;
		      }
		      break;
		  }
		}
		goto store_contents;
	    }

	    // otherwise, ascii
	    // first, parse contents
	    switch (_contents[i]) {

	      case W_NONE:
		while (data < end && !isspace(*data))
		    data++;
		break;

	      case W_TIMESTAMP:
	      case W_NTIMESTAMP:
	      case W_FIRST_TIMESTAMP:
	      case W_FIRST_NTIMESTAMP:
		next = cp_unsigned(data, end, 10, &u1);
		if (next > data) {
		    data = next;
		    if (data + 1 < end && *data == '.') {
			int digit = 0;
			for (data++; digit < 9 && data < end && isdigit(*data); digit++, data++)
			    u2 = (u2 * 10) + *data - '0';
			for (; digit < 9; digit++)
			    u2 = (u2 * 10);
			for (; data < end && isdigit(*data); data++)
			    /* nada */;
		    }
		}
		break;
		
	      case W_TIMESTAMP_SEC:
	      case W_TIMESTAMP_USEC:
	      case W_IP_LEN:
	      case W_PAYLOAD_LEN:
	      case W_IP_CAPTURE_LEN:
	      case W_IP_ID:
	      case W_SPORT:
	      case W_DPORT:
	      case W_TCP_SEQ:
	      case W_TCP_ACK:
	      case W_COUNT:
	      case W_AGGREGATE:
	      case W_TCP_WINDOW:
	      case W_TCP_URP:
	      case W_IP_TOS:
	      case W_IP_TTL:
		data = cp_unsigned(data, end, 0, &u1);
		break;

	      case W_TIMESTAMP_USEC1: {
#if HAVE_INT64_TYPES
		  uint64_t uu;
		  data = cp_unsigned(data, end, 0, &uu);
		  u1 = (uint32_t)(uu >> 32);
		  u2 = (uint32_t) uu;
#else
		  // silently truncate large numbers
		  data = cp_unsigned(data, end, 0, &u2);
#endif
		  break;
	      }
		
	      case W_IP_SRC:
	      case W_IP_DST:
		for (int j = 0; j < 4; j++) {
		    const unsigned char *first = data;
		    int x = 0;
		    while (data < end && isdigit(*data) && x < 256)
			(x = (x * 10) + *data - '0'), data++;
		    if (x >= 256 || data == first || (j < 3 && (data >= end || *data != '.'))) {
			data = original_data;
			break;
		    }
		    u1 = (u1 << 8) + x;
		    if (j < 3)
			data++;
		}
		break;

	      case W_IP_PROTO:
		if (*data == 'T') {
		    u1 = IP_PROTO_TCP;
		    data++;
		} else if (*data == 'U') {
		    u1 = IP_PROTO_UDP;
		    data++;
		} else if (*data == 'I') {
		    u1 = IP_PROTO_ICMP;
		    data++;
		} else
		    data = cp_unsigned(data, end, 0, &u1);
		break;

	      case W_IP_FRAG:
		if (*data == 'F') {
		    u1 = htons(IP_MF);
		    data++;
		} else if (*data == 'f') {
		    u1 = htons(100);	// random number
		    data++;
		} else if (*data == '.')
		    data++;	// u1 already 0
		break;

	      case W_IP_FRAGOFF:
		next = cp_unsigned(data, end, 0, &u1);
		if (_minor_version == 0) // old-style file
		    u1 <<= 3;
		if (next > data && (u1 & 7) == 0 && u1 < 65536) {
		    u1 >>= 3;
		    data = next;
		    if (data < end && *data == '+') {
			u1 |= IP_MF;
			data++;
		    }
		}
		break;

	      case W_TCP_FLAGS:
		if (isdigit(*data))
		    data = cp_unsigned(data, end, 0, &u1);
		else if (*data == '.')
		    data++;
		else
		    while (data < end && IPSummaryDump::tcp_flag_mapping[*data]) {
			u1 |= 1 << (IPSummaryDump::tcp_flag_mapping[*data] - 1);
			data++;
		    }
		break;

	      case W_IP_OPT:
		if (*data == '.')
		    data++;
		else if (*data != '-') {
		    have_ip_opt = true;
		    data = parse_ip_opt_ascii(data, end, &ip_opt, DO_IPOPT_ALL);
		}
		break;

	      case W_TCP_SACK:
		if (*data == '.')
		    data++;
		else if (*data != '-') {
		    have_tcp_opt = true;
		    data = parse_tcp_opt_ascii(data, end, &tcp_opt, DO_TCPOPT_SACK);
		}
		break;
		
	      case W_TCP_NTOPT:
		if (*data == '.')
		    data++;
		else if (*data != '-') {
		    have_tcp_opt = true;
		    data = parse_tcp_opt_ascii(data, end, &tcp_opt, DO_TCPOPT_NTALL);
		}
		break;
		
	      case W_TCP_OPT:
		if (*data == '.')
		    data++;
		else if (*data != '-') {
		    have_tcp_opt = true;
		    data = parse_tcp_opt_ascii(data, end, &tcp_opt, DO_TCPOPT_ALL);
		}
		break;
		
	      case W_LINK:
		if (*data == '>' || *data == 'L') {
		    u1 = 0;
		    data++;
		} else if (*data == '<' || *data == 'X' || *data == 'R') {
		    u1 = 1;
		    data++;
		} else
		    data = cp_unsigned(data, end, 0, &u1);
		break;

	      case W_PAYLOAD:
		if (*data == '\"') {
		    payload.clear();
		    const unsigned char *fdata = data + 1;
		    for (data++; data < end && *data != '\"'; data++)
			if (*data == '\\' && data < end - 1) {
			    payload.append((const char *) fdata, (const char *) data);
			    fdata = (const unsigned char *) cp_process_backslash((const char *) data, (const char *) end, payload);
			    data = fdata - 1; // account for loop increment
			}
		    payload.append((const char *) fdata, (const char *) data);
		    // bag payload if it didn't parse correctly
		    if (data >= end || *data != '\"')
			data = original_data;
		    else {
			have_payload = have_payload_len = true;
			payload_len = payload.length();
		    }
		}
		break;

	    }

	    // check whether we correctly parsed something
	    {
		bool this_ok = (data > original_data && (data >= end || isspace(*data)));
		while (data < end && !isspace(*data))
		    data++;
		while (data < end && isspace(*data))
		    data++;
		if (!this_ok)
		    continue;
	    }

	    // store contents
	  store_contents:
	    switch (_contents[i]) {

	      case W_TIMESTAMP:
	      case W_NTIMESTAMP:
		if (u2 < 1000000000)
		    q->timestamp_anno().set_nsec(u1, u2), ok++;
		break;

	      case W_TIMESTAMP_SEC:
		q->timestamp_anno().set_sec(u1), ok++;
		break;

	      case W_TIMESTAMP_USEC:
		if (u1 < 1000000)
		    q->timestamp_anno().set_subsec(Timestamp::usec_to_subsec(u1)), ok++;
		break;

	      case W_TIMESTAMP_USEC1:
		if (u1 == 0 && u2 < 1000000)
		    q->timestamp_anno().set_usec(0, u2), ok++;
		else if (u1 == 0)
		    q->timestamp_anno().set_usec(u2/1000000, u2%1000000), ok++;
#if HAVE_INT64_TYPES
		else {
		    uint64_t uu = ((uint64_t)u1 << 32) | u2;
		    q->timestamp_anno().set_usec(uu/1000000, uu%1000000), ok++;
		}
#endif
		break;
		
	      case W_IP_SRC:
		iph->ip_src.s_addr = htonl(u1), ip_ok++;
		break;

	      case W_IP_DST:
		iph->ip_dst.s_addr = htonl(u1), ip_ok++;
		break;
		
	      case W_IP_LEN:
		if (u1 <= 0xFFFF)
		    byte_count = u1, ok++;
		break;
		
	      case W_PAYLOAD_LEN:
		if (u1 <= 0xFFFF)
		    payload_len = u1, have_payload_len = true, ok++;
		break;

	      case W_IP_CAPTURE_LEN:
		/* XXX do nothing with this for now */
		ok++;
		break;
		
	      case W_IP_PROTO:
		if (u1 <= 255)
		    iph->ip_p = u1, ip_ok++;
		break;

	      case W_IP_TOS:
		if (u1 <= 255)
		    iph->ip_tos = u1, ip_ok++;
		break;

	      case W_IP_TTL:
		if (u1 <= 255)
		    iph->ip_ttl = u1, ip_ok++;
		break;

	      case W_IP_ID:
		if (u1 <= 0xFFFF)
		    iph->ip_id = htons(u1), ip_ok++;
		break;

	      case W_IP_FRAG:
		iph->ip_off = u1, ip_ok++;
		break;

	      case W_IP_FRAGOFF:
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
		else if (u1 <= 0xFFF)
		    // th_off will be set later
		    *reinterpret_cast<uint16_t *>(q->transport_header() + 12) = htons(u1), ip_ok++;
		break;

	      case W_TCP_WINDOW:
		if (u1 <= 0xFFFF)
		    q->tcp_header()->th_win = htons(u1), ip_ok++;
		break;
		
	      case W_TCP_URP:
		if (u1 <= 0xFFFF)
		    q->tcp_header()->th_urp = htons(u1), ip_ok++;
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
	      case W_FIRST_NTIMESTAMP:
		if (u2 < 1000000000) {
		    SET_FIRST_TIMESTAMP_ANNO(q, Timestamp::make_nsec(u1, u2));
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

	// set IP length
	if (have_payload) {	// XXX what if byte_count indicates IP options?
	    int old_length = q->length();
	    iph->ip_len = ntohs(old_length + payload.length());
	    if ((q = q->put(payload.length()))) {
		memcpy(q->data() + old_length, payload.data(), payload.length());
		// iph may have changed!!
		iph = q->ip_header();
	    }
	} else if (byte_count) {
	    iph->ip_len = ntohs(byte_count);
	    SET_EXTRA_LENGTH_ANNO(q, byte_count - q->length());
	} else if (have_payload_len) {
	    iph->ip_len = ntohs(q->length() + payload_len);
	    SET_EXTRA_LENGTH_ANNO(q, payload_len);
	} else
	    iph->ip_len = ntohs(q->length());

	// set UDP length (after IP length is available)
	if (iph->ip_p == IP_PROTO_UDP && IP_FIRSTFRAG(iph))
	    q->udp_header()->uh_ulen = htons(ntohs(iph->ip_len) - (iph->ip_hl << 2));
	
	// set data from flow ID
	if (_use_flowid) {
	    IPFlowID flowid = (PAINT_ANNO(q) & 1 ? _flowid.rev() : _flowid);
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

	// set destination IP address annotation
	q->set_dst_ip_anno(iph->ip_dst);

	// set checksum
	if (_checksum && ip_ok)
	    set_checksums(q, iph);
	
	return q;
    }

    // bad format if we get here
    if (!_format_complaint) {
	// don't complain if the line was all blank
	if ((int) strspn(line.data(), " \t\n\r") != line.length()) {
	    if (_contents.size() == 0)
		_ff.error(errh, "no '!data' provided");
	    else
		_ff.error(errh, "packet parse error");
	    _format_complaint = true;
	}
    }
    if (q)
	q->kill();
    return 0;
}

inline Packet *
set_packet_lengths(Packet *p, uint32_t extra_length)
{
    uint32_t length = p->length() + extra_length;
    if (htons(length) != p->ip_header()->ip_len) {
	if (WritablePacket *q = p->uniqueify()) {
	    click_ip *ip = q->ip_header();
	    ip->ip_len = htons(length);
	    if (ip->ip_p == IP_PROTO_UDP)
		q->udp_header()->uh_ulen = htons(length - (ip->ip_hl << 2));
	    return q;
	} else
	    return 0;
    } else
	return p;
}

Packet *
FromIPSummaryDump::handle_multipacket(Packet *p)
{
    assert(!_work_packet || _work_packet == p);
    
    if (!p || !EXTRA_PACKETS_ANNO(p)) {
	_work_packet = 0;
	return p;
    }

    uint32_t count = 1 + EXTRA_PACKETS_ANNO(p);

    // set up _multipacket variables on new packets (_work_packet == 0)
    if (!_work_packet) {
	assert(count > 1);
	// set length of all but the last packet
	_multipacket_length = (p->length() + EXTRA_LENGTH_ANNO(p)) / count;
	// beware if there isn't enough EXTRA_LENGTH to cover all the packets
	if (_multipacket_length < p->length()) {
	    _multipacket_length = p->length();
	    SET_EXTRA_LENGTH_ANNO(p, _multipacket_length * (count - 1));
	}
	// set timestamps
	_multipacket_end_timestamp = p->timestamp_anno();
	if (FIRST_TIMESTAMP_ANNO(p)) {
	    _multipacket_timestamp_delta = (p->timestamp_anno() - FIRST_TIMESTAMP_ANNO(p)) / (count - 1);
	    p->timestamp_anno() = FIRST_TIMESTAMP_ANNO(p);
	} else
	    _multipacket_timestamp_delta = Timestamp();
	// prepare IP lengths for _multipacket_extra_length
	_work_packet = set_packet_lengths(p, _multipacket_length - p->length());
	if (!_work_packet)
	    return 0;
    }

    // prepare packet to return
    if ((p = p->clone())) {
	SET_EXTRA_PACKETS_ANNO(p, 0);
	SET_EXTRA_LENGTH_ANNO(p, _multipacket_length - p->length());
    }

    // reduce weight of _work_packet 
    SET_EXTRA_PACKETS_ANNO(_work_packet, count - 2);
    SET_EXTRA_LENGTH_ANNO(_work_packet, EXTRA_LENGTH_ANNO(_work_packet) - _multipacket_length);
    if (count == 2) {
	_work_packet->timestamp_anno() = _multipacket_end_timestamp;
	_work_packet = set_packet_lengths(_work_packet, EXTRA_LENGTH_ANNO(_work_packet));
    } else
	_work_packet->timestamp_anno() += _multipacket_timestamp_delta;

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
	    _notifier.sleep();
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

    _notifier.wake();
    return p;
}


enum { H_SAMPLING_PROB, H_ACTIVE, H_ENCAP, H_STOP };

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
		  fd->_notifier.set_active(active, true);
	      return 0;
	  } else
	      return errh->error("'active' should be Boolean");
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
    add_write_handler("stop", write_handler, (void *)H_STOP);
    _ff.add_handlers(this);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel FromFile IPSummaryDumpInfo)
EXPORT_ELEMENT(FromIPSummaryDump)
CLICK_ENDDECLS
