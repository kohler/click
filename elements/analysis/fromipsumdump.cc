// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromipsumdump.{cc,hh} -- element reads packets from IP summary dump file
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2008 Meraki, Inc.
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
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <clicknet/icmp.h>
#include <click/packet_anno.hh>
#include <click/nameinfo.hh>
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
    : _work_packet(0), _task(this), _timer(this)
{
    _ff.set_landmark_pattern("%f:%l");
}

FromIPSummaryDump::~FromIPSummaryDump()
{
}

void *
FromIPSummaryDump::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0 && !output_is_push(0))
	return static_cast<Notifier *>(&_notifier);
    else
	return Element::cast(n);
}

int
FromIPSummaryDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true, zero = true, checksum = false, multipacket = false, timing = false, allow_nonexistent = false;
    uint8_t default_proto = IP_PROTO_TCP;
    _sampling_prob = (1 << SAMPLING_SHIFT);
    String default_contents, default_flowid, data;

    if (Args(conf, this, errh)
	.read_p("FILENAME", FilenameArg(), _ff.filename())
	.read("STOP", stop)
	.read("ACTIVE", active)
	.read("ZERO", zero)
	.read("TIMING", timing)
	.read("CHECKSUM", checksum)
	.read("SAMPLE", FixedPointArg(SAMPLING_SHIFT), _sampling_prob)
	.read("PROTO", default_proto)
	.read("MULTIPACKET", multipacket)
	.read("DEFAULT_CONTENTS", AnyArg(), default_contents)
	.read("DEFAULT_FIELDS", AnyArg(), default_contents)
	.read("DEFAULT_FLOWID", AnyArg(), default_flowid)
	.read("CONTENTS", AnyArg(), default_contents)
	.read("FIELDS", AnyArg(), default_contents)
	.read("FLOWID", AnyArg(), default_flowid)
	.read("ALLOW_NONEXISTENT", allow_nonexistent)
        .read("DATA", data)
	.complete() < 0)
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
    _timing = timing;
    _allow_nonexistent = allow_nonexistent;
    _have_timing = false;
    _multipacket = multipacket;
    _have_flowid = _have_aggregate = _binary = false;
    if (default_contents)
	bang_data(default_contents, errh);
    if (default_flowid)
	bang_flowid(default_flowid, errh);
    if (data && _ff.filename())
        return errh->error("FILENAME and DATA conflict");
    else if (data && _ff.set_data(data, errh) < 0)
        return -1;
    else if (!_ff.filename())
        return errh->error("FILENAME: required argument missing");
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
	_notifier.initialize(Notifier::EMPTY_NOTIFIER, router());
    _timer.initialize(this);
    _format_complaint = false;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);

    int e = _ff.initialize(errh, _allow_nonexistent);
    if (e == -ENOENT && _allow_nonexistent)
	return 0;
    else if (e < 0)
	return e;

    _minor_version = IPSummaryDump::MINOR_VERSION; // expected minor version
    String line;
    if (_ff.peek_line(line, errh, true) < 0)
	return -1;
    else if (line.substring(0, 14) == "!IPSummaryDump") {
	int major_version;
	if (sscanf(line.c_str() + 14, " %d.%d", &major_version, &_minor_version) == 2) {
	    if (major_version != IPSummaryDump::MAJOR_VERSION || _minor_version > IPSummaryDump::MINOR_VERSION) {
		_ff.warning(errh, "unexpected IPSummaryDump version %d.%d", major_version, _minor_version);
		_minor_version = IPSummaryDump::MINOR_VERSION;
	    }
	}
	(void) _ff.read_line(line, errh, true); // throw away line
    } else {
	// parse line again, warn if this doesn't look like a dump
	if (!line.substring(0, 8).equals("!creator", 8)
	    && !line.substring(0, 5).equals("!data", 5)
	    && !line.substring(0, 9).equals("!contents", 9)
	    && !line.substring(0, 6).equals("!proto", 6)
	    && !line.substring(0, 7).equals("!flowid", 7)) {
	    if (!_fields.size() /* don't warn on DEFAULT_CONTENTS */)
		_ff.warning(errh, "missing banner line; is this an IP summary dump?");
	}
    }

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

int
FromIPSummaryDump::sort_fields_compare(const void *ap, const void *bp,
				       void *user_data)
{
    int a = *reinterpret_cast<const int *>(ap);
    int b = *reinterpret_cast<const int *>(bp);
    FromIPSummaryDump *f = reinterpret_cast<FromIPSummaryDump *>(user_data);
    const IPSummaryDump::FieldReader *fa = f->_fields[a];
    const IPSummaryDump::FieldReader *fb = f->_fields[b];
    if (fa->order < fb->order)
	return -1;
    if (fa->order > fb->order)
	return 1;
    return (a < b ? -1 : (a == b ? 0 : 1));
}

void
FromIPSummaryDump::bang_data(const String &line, ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(line, words);

    _fields.clear();
    _field_order.clear();
    for (int i = 0; i < words.size(); i++) {
	String word = cp_unquote(words[i]);
	if (i == 0 && (word == "!data" || word == "!contents"))
	    continue;
	const IPSummaryDump::FieldReader *f = IPSummaryDump::FieldReader::find(word);
	if (!f) {
	    _ff.warning(errh, "unknown content type '%s'", word.c_str());
	    f = &IPSummaryDump::null_reader;
	} else if (!f->inject) {
	    _ff.warning(errh, "content type '%s' ignored on input", word.c_str());
	    f = &IPSummaryDump::null_reader;
	}
	_fields.push_back(f);
	_field_order.push_back(_fields.size() - 1);
    }

    if (_fields.size() == 0)
	_ff.error(errh, "no contents specified");

    click_qsort(_field_order.begin(), _fields.size(), sizeof(int),
		sort_fields_compare, this);
}

void
FromIPSummaryDump::bang_proto(const String &line, const char *type,
			      ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(line, words);
    int32_t proto;

    if (words.size() != 2)
	_ff.error(errh, "bad %s", type);
    else if (NameInfo::query_int(NameInfo::T_IP_PROTO, this, words[1], &proto)
	     && proto < 256)
	_default_proto = proto;
    else if (words[1] == "T")
	_default_proto = IP_PROTO_TCP;
    else if (words[1] == "U")
	_default_proto = IP_PROTO_UDP;
    else if (words[1] == "I")
	_default_proto = IP_PROTO_ICMP;
    else
	_ff.error(errh, "bad protocol in %s", type);
}

void
FromIPSummaryDump::bang_flowid(const String &line, ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(line, words);

    IPAddress src, dst;
    uint32_t sport = 0, dport = 0;
    if (words.size() < 5
	|| (!IPAddressArg().parse(words[1], src) && words[1] != "-")
	|| (!IntArg().parse(words[2], sport) && words[2] != "-")
	|| (!IPAddressArg().parse(words[3], dst) && words[3] != "-")
	|| (!IntArg().parse(words[4], dport) && words[4] != "-")
	|| sport > 65535 || dport > 65535) {
	_ff.error(errh, "bad !flowid specification");
	_have_flowid = false;
    } else {
	if (words.size() >= 6)
	    bang_proto(String::make_stable("! ", 2) + words[5], "!flowid", errh);
	_given_flowid = IPFlowID(src, htons(sport), dst, htons(dport));
	_have_flowid = true;
    }
}

void
FromIPSummaryDump::bang_aggregate(const String &line, ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(line, words);

    if (words.size() != 2
	|| !IntArg().parse(words[1], _aggregate)) {
	_ff.error(errh, "bad !aggregate specification");
	_have_aggregate = false;
    } else
	_have_aggregate = true;
}

void
FromIPSummaryDump::bang_binary(const String &line, ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(line, words);
    if (words.size() != 1)
	_ff.error(errh, "bad !binary specification");
    _binary = true;
    _ff.set_landmark_pattern("%f:record %l");
    _ff.set_lineno(1);
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
    } else if (iph->ip_p == IP_PROTO_ICMP) {
	click_icmp *icmph = q->icmp_header();
	icmph->icmp_cksum = 0;
	icmph->icmp_cksum = click_in_cksum((const uint8_t *) icmph, q->transport_length());
    }
}

Packet *
FromIPSummaryDump::read_packet(ErrorHandler *errh)
{
    // read non-packet lines
    bool binary;
    String line;
    const char *data;
    const char *end;

    while (1) {
	if ((binary = _binary)) {
	    int result = read_binary(line, errh);
	    if (result <= 0)
		goto eof;
	    else
		binary = (result == 1);
	} else if (_ff.read_line(line, errh, true) <= 0) {
	  eof:
	    _ff.cleanup();
	    return 0;
	}

	data = line.begin();
	end = line.end();

	if (data == end)
	    /* do nothing */;
	else if (binary || (data[0] != '!' && data[0] != '#'))
	    /* real packet */
	    break;

	// parse bang lines
	if (data[0] == '!') {
	    if (data + 6 <= end && memcmp(data, "!data", 5) == 0 && isspace((unsigned char) data[5]))
		bang_data(line, errh);
	    else if (data + 8 <= end && memcmp(data, "!flowid", 7) == 0 && isspace((unsigned char) data[7]))
		bang_flowid(line, errh);
	    else if (data + 7 <= end && memcmp(data, "!proto", 6) == 0 && isspace((unsigned char) data[6]))
		bang_proto(line, "!proto", errh);
	    else if (data + 11 <= end && memcmp(data, "!aggregate", 10) == 0 && isspace((unsigned char) data[10]))
		bang_aggregate(line, errh);
	    else if (data + 8 <= end && memcmp(data, "!binary", 7) == 0 && isspace((unsigned char) data[7]))
		bang_binary(line, errh);
	    else if (data + 10 <= end && memcmp(data, "!contents", 9) == 0 && isspace((unsigned char) data[9]))
		bang_data(line, errh);
	}
    }

    // read packet data
    WritablePacket *q = Packet::make(16, (const unsigned char *) 0, 0, 1000);
    if (!q) {
	_ff.error(errh, strerror(ENOMEM));
	return 0;
    }
    if (_zero)
	memset(q->buffer(), 0, q->buffer_length());

    // prepare packet data
    StringAccum sa;
    IPSummaryDump::PacketOdesc d(this, q, _default_proto, (_have_flowid ? &_flowid : 0), _minor_version);
    int nfields = 0;

    // new code goes here
    if (_binary) {
	Vector<const unsigned char *> args;
	int nbytes;
	for (const IPSummaryDump::FieldReader * const *fp = _fields.begin(); fp != _fields.end(); ++fp) {
	    if (!(*fp)->inb)
		goto bad_field;
	    switch ((*fp)->type) {
	      case IPSummaryDump::B_0:
		nbytes = 0;
		goto got_nbytes;
	      case IPSummaryDump::B_1:
		nbytes = 1;
		goto got_nbytes;
	      case IPSummaryDump::B_2:
		nbytes = 2;
		goto got_nbytes;
	      case IPSummaryDump::B_4:
	      case IPSummaryDump::B_4NET:
		nbytes = 4;
		goto got_nbytes;
	      case IPSummaryDump::B_6PTR:
		nbytes = 6;
		goto got_nbytes;
	      case IPSummaryDump::B_8:
		nbytes = 8;
		goto got_nbytes;
	      case IPSummaryDump::B_16:
		nbytes = 16;
		goto got_nbytes;
	      got_nbytes:
		if (data + nbytes <= end) {
		    args.push_back((const unsigned char *) data);
		    data += nbytes;
		} else
		    goto bad_field;
		break;
	      case IPSummaryDump::B_SPECIAL:
		args.push_back((const unsigned char *) data);
		data = (const char *) (*fp)->inb(d, (const uint8_t *) data, (const uint8_t *) end, *fp);
		break;
	      bad_field:
	      default:
		args.push_back(0);
		data = end;
		break;
	    }
	}

	for (int *fip = _field_order.begin();
	     fip != _field_order.end() && d.p;
	     ++fip) {
	    const IPSummaryDump::FieldReader *f = _fields[*fip];
	    if (!args[*fip] || !f->inject)
		continue;
	    d.clear_values();
	    if (f->inb(d, args[*fip], (const uint8_t *) end, f)) {
		f->inject(d, f);
		nfields++;
	    }
	}

    } else {
	Vector<String> args;
	while (args.size() < _fields.size()) {
	    const char *original_data = data;
	    while (data < end)
		if (isspace((unsigned char) *data))
		    break;
		else if (*data == '\"')
		    data = cp_skip_double_quote(data, end);
		else
		    ++data;
	    args.push_back(line.substring(original_data, data));
	    while (data < end && isspace((unsigned char) *data))
		++data;
	}

	for (int *fip = _field_order.begin();
	     fip != _field_order.end() && d.p;
	     ++fip) {
	    const IPSummaryDump::FieldReader *f = _fields[*fip];
	    if (!args[*fip] || args[*fip].equals("-", 1) || !f->inject)
		continue;
	    d.clear_values();
	    if (f->ina(d, args[*fip], f)) {
		f->inject(d, f);
		nfields++;
	    }
	}
    }

    if (!nfields) {	// bad format
	if (!_format_complaint) {
	    // don't complain if the line was all blank
	    if (binary || !cp_is_space(line)) {
		if (_fields.size() == 0)
		    _ff.error(errh, "no '!data' provided");
		else
		    _ff.error(errh, "packet parse error");
		_format_complaint = true;
	    }
	}
	if (d.p)
	    d.p->kill();
	d.p = 0;
    }

    // set source and destination ports even if no transport info on packet
    if (d.p && d.default_ip_flowid)
	(void) d.make_ip(0);	// may fail

    // set up transport header if necessary
    if (d.p && d.is_ip && d.p->ip_header())
	(void) d.make_transp();

    if (d.p && d.is_ip && d.p->ip_header()) {
	// set IP length
	uint32_t ip_len;
	if (!d.p->ip_header()->ip_len) {
	    ip_len = d.want_len;
	    if (ip_len >= (uint32_t) d.p->network_header_offset())
		ip_len -= d.p->network_header_offset();
	    if (ip_len > 0xFFFF)
		ip_len = 0xFFFF;
	    else if (ip_len == 0)
		ip_len = d.p->network_length();
	    d.p->ip_header()->ip_len = htons(ip_len);
	} else
	    ip_len = ntohs(d.p->ip_header()->ip_len);

	// set UDP length
	if (d.p->ip_header()->ip_p == IP_PROTO_UDP
	    && IP_FIRSTFRAG(d.p->ip_header())
	    && !d.p->udp_header()->uh_ulen) {
	    int len = ip_len - d.p->network_header_length();
	    d.p->udp_header()->uh_ulen = htons(len);
	}

	// set destination IP address annotation
	d.p->set_dst_ip_anno(d.p->ip_header()->ip_dst);

	// set checksum
	if (_checksum) {
	    uint32_t xlen = 0;
	    if (ip_len > (uint32_t) d.p->network_length())
		xlen = ip_len - d.p->network_length();
	    if (!xlen || (d.p = d.p->put(xlen))) {
		if (xlen && _zero)
		    memset(d.p->end_data() - xlen, 0, xlen);
		set_checksums(d.p, d.p->ip_header());
	    }
	}
    }

    // set extra length annotation (post-other length adjustments)
    if (d.p && d.want_len > d.p->length())
	SET_EXTRA_LENGTH_ANNO(d.p, d.want_len - d.p->length());

    return d.p;
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

void
FromIPSummaryDump::run_timer(Timer *)
{
    if (_active) {
	if (output_is_pull(0))
	    _notifier.wake();
	else
	    _task.reschedule();
    }
}

bool
FromIPSummaryDump::check_timing(Packet *p)
{
    assert(!_work_packet || _work_packet == p);
    Timestamp now_s = Timestamp::now_steady();
    if (!_have_timing) {
	_timing_offset = now_s - p->timestamp_anno();
	_have_timing = true;
    }
    Timestamp t = p->timestamp_anno() + _timing_offset;
    if (now_s < t) {
	t -= Timer::adjustment();
	if (now_s < t) {
	    _timer.schedule_at_steady(t);
	    if (output_is_pull(0))
		_notifier.sleep();
	} else {
	    if (output_is_push(0))
		_task.fast_reschedule();
	}
	_work_packet = p;
	return false;
    }
    _work_packet = 0;
    return true;
}

bool
FromIPSummaryDump::run_task(Task *)
{
    if (!_active)
	return false;
    Packet *p;

    while (1) {
	p = (_work_packet ? _work_packet : read_packet(0));
	if (!p && !_ff.initialized()) {
	    if (_stop)
		router()->please_stop_driver();
	    return false;
	} else if (!p)
	    break;
	if (p && _timing && !check_timing(p))
	    return false;
	if (_multipacket)
	    p = handle_multipacket(p);
	// check sampling probability
	if (_sampling_prob >= (1 << SAMPLING_SHIFT)
	    || (click_random() & ((1 << SAMPLING_SHIFT) - 1)) < _sampling_prob)
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
	if (!p && !_ff.initialized()) {
	    if (_stop)
		router()->please_stop_driver();
	    _notifier.sleep();
	    return 0;
	}
	if (p && _timing && !check_timing(p))
	    return 0;
	if (_multipacket)
	    p = handle_multipacket(p);
	// check sampling probability
	if (_sampling_prob >= (1 << SAMPLING_SHIFT)
	    || (click_random() & ((1 << SAMPLING_SHIFT) - 1)) < _sampling_prob)
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
	return cp_unparse_real2(fd->_sampling_prob, SAMPLING_SHIFT);
      case H_ACTIVE:
	return BoolArg::unparse(fd->_active);
      case H_ENCAP:
	return "IP";
      default:
	return "<error>";
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
	  if (BoolArg().parse(s, active)) {
	      fd->_active = active;
	      if (fd->output_is_push(0) && active && !fd->_task.scheduled())
		  fd->_task.reschedule();
	      else if (!fd->output_is_push(0))
		  fd->_notifier.set_active(active, true);
	      return 0;
	  } else
	      return errh->error("type mismatch");
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
    add_read_handler("sampling_prob", read_handler, H_SAMPLING_PROB);
    add_read_handler("active", read_handler, H_ACTIVE, Handler::f_checkbox);
    add_write_handler("active", write_handler, H_ACTIVE);
    add_read_handler("encap", read_handler, H_ENCAP);
    add_write_handler("stop", write_handler, H_STOP, Handler::f_button);
    _ff.add_handlers(this);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel IPSummaryDumpInfo)
EXPORT_ELEMENT(FromIPSummaryDump)
CLICK_ENDDECLS
