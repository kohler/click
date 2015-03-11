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
#include <click/args.hh>
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
    : _task(this)
{
    _ff.set_landmark_pattern("%f:%l");
}

FromTcpdump::~FromTcpdump()
{
}

void *
FromTcpdump::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0 && !output_is_push(0))
	return static_cast<Notifier *>(&_notifier);
    else
	return Element::cast(n);
}

int
FromTcpdump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true, zero = true, checksum = false;
    _sampling_prob = (1 << SAMPLING_SHIFT);
    _absolute_seq = -1;

    if (Args(conf, this, errh)
	.read_mp("FILENAME", FilenameArg(), _ff.filename())
	.read("STOP", stop)
	.read("ACTIVE", active)
	.read("ZERO", zero)
	.read("CHECKSUM", checksum)
	.read("SAMPLE", FixedPointArg(SAMPLING_SHIFT), _sampling_prob)
	.complete() < 0)
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
FromTcpdump::initialize(ErrorHandler *errh)
{
    // make sure notifier is initialized
    if (!output_is_push(0))
	_notifier.initialize(Notifier::EMPTY_NOTIFIER, router());

    if (_ff.initialize(errh) < 0)
	return -1;

    // read a line
    String line;
    if (_ff.peek_line(line, errh, true) < 0)
	return -1;
    else if (!line || !isdigit((unsigned char) line[0]))
	errh->lwarning(_ff.print_filename(), "first line suspicious; is this a tcpdump output file?");

    _format_complaint = false;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return 0;
}

void
FromTcpdump::cleanup(CleanupStage)
{
    _ff.cleanup();
}

static void
append_net_uint32_t(StringAccum &sa, uint32_t u)
{
    sa << (char)(u >> 24) << (char)(u >> 16) << (char)(u >> 8) << (char)u;
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

const char *
FromTcpdump::read_tcp_line(WritablePacket *&q, const char *s, const char *end, int *data_len)
{
    click_tcp *tcph = q->tcp_header();

    // first, read flags
    if (s < end && *s == '.')
	tcph->th_flags = TH_ACK, s++;
    else {
	tcph->th_flags = 0;
	while (s < end && IPSummaryDump::tcp_flag_mapping[(uint8_t) *s]) {
	    tcph->th_flags |= (1 << (IPSummaryDump::tcp_flag_mapping[ (uint8_t) *s ] - 1));
	    s++;
	}
    }
    if (s >= end || *s != ' ')
	return s;

    // second, check for '[bad hdr length]' '[tcp sum ok]'
    const char *s2;
    if (s + 9 <= end && memcmp(s + 1, "[bad hdr", 8) == 0 && (s2 = find(s + 9, end, ']')) + 1 < end && s2[1] == ' ')
	s = s2 + 1;
    if (s + 14 <= end && memcmp(s + 1, "[tcp sum ok] ", 13) == 0)
	s += 14;
    else if (s + 9 <= end && memcmp(s + 1, "[bad tcp", 8) == 0 && (s2 = find(s + 9, end, ']')) + 1 < end && s2[1] == ' ')
	s = s2 + 2;
    else
	s++;

    // then read sequence numbers
    uint32_t seq = 0, end_seq = 0, ack_seq = 0;
    if (s < end && s[0] != 'a') {
	const char *eseq = cp_integer(s, end, 0, &seq);
	if (eseq == s || eseq >= end || *eseq != ':')
	    return s;
	const char *eend_seq = cp_integer(eseq + 1, end, 0, &end_seq);
	if (eend_seq == eseq + 1 || eend_seq >= end || *eend_seq != '(')
	    return s;
	// skip parenthesized length
	for (s = eend_seq + 1; s < end && isdigit((unsigned char) *s); s++)
	    /* nada */;
	if (s >= end || *s != ')')
	    return s;
	else if (s + 1 >= end || s[1] != ' ')
	    return s + 1;
	else
	    s += 2;
    }
    *data_len = end_seq - seq;

    // check for 'ack'
    if (s + 4 < end && s[0] == 'a' && s[1] == 'c' && s[2] == 'k' && s[3] == ' ' && isdigit((unsigned char) s[4])) {
	tcph->th_flags |= TH_ACK;
	s = cp_integer(s + 4, end, 0, &ack_seq);
	if (s < end && *s == ' ')
	    s++;
    }

    // patch sequence numbers
    FlowRecord *record = 0;
    bool rev = false;
    if (tcph->th_flags & TH_ACK) {
	// first, look up a record for this flow
	const click_ip *iph = q->ip_header();
	rev = (tcph->th_sport < tcph->th_dport
	       || (tcph->th_sport == tcph->th_dport && iph->ip_src.s_addr < iph->ip_dst.s_addr));
	if (rev)
	    record = &_tcp_map[IPFlowID(iph->ip_src, tcph->th_sport, iph->ip_dst, tcph->th_dport)];
	else
	    record = &_tcp_map[IPFlowID(iph->ip_dst, tcph->th_dport, iph->ip_src, tcph->th_sport)];
	// use tcpdump's heuristic for deciding when this is a new flow
	if ((!record->init_seq[0] && !record->init_seq[1]) || (tcph->th_flags & TH_SYN)) {
	    record->init_seq[rev] = seq;
	    record->init_seq[!rev] = ack_seq - 1;
	} else {
	    if (_absolute_seq < 0) // heuristic
		_absolute_seq = (SEQ_GEQ(ack_seq, record->init_seq[!rev]) && ack_seq > 8000);
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
    if (s + 4 < end && s[0] == 'w' && s[1] == 'i' && s[2] == 'n' && s[3] == ' ' && isdigit((unsigned char) s[4])) {
	s = cp_integer(s + 4, end, 0, &u); // XXX check u <= 65535
	tcph->th_win = htons(u);
	if (s < end && *s == ' ')
	    s++;
    }

    // check for 'urg'
    if (s + 4 < end && s[0] == 'u' && s[1] == 'r' && s[2] == 'g' && s[3] == ' ' && isdigit((unsigned char) s[4])) {
	s = cp_integer(s + 4, end, 0, &u); // XXX check u <= 65535
	tcph->th_urp = htons(u);
	if (s < end && *s != ' ')
	    s++;
    }

    // check for options
    StringAccum opt;
    if (s < end && s[0] == '<') {
	for (s++; s < end && *s != '>'; ) {
	    int optlen1 = opt.length();
	    if (s + 3 <= end && s[0] == 'n' && s[1] == 'o' && s[2] == 'p') {
		opt << (char)TCPOPT_NOP;
		s += 3;
	    } else if (s + 3 <= end && s[0] == 'e' && s[1] == 'o' && s[2] == 'l') {
		opt << (char)TCPOPT_EOL;
		s += 3;
	    } else if (s + 4 < end && s[0] == 'm' && s[1] == 's' && s[2] == 's' && s[3] == ' ' && isdigit((unsigned char) s[4])) {
		s = cp_integer(s + 4, end, 0, &u); // XXX check u <= 65535
		opt << (char)TCPOPT_MAXSEG << (char)TCPOLEN_MAXSEG << (char)((u >> 8) & 255) << (char)(u & 255);
	    } else if (s + 7 < end && memcmp(s, "wscale ", 7) == 0 && isdigit((unsigned char) s[7])) {
		s = cp_integer(s + 7, end, 0, &u); // XXX check u <= 255
		opt << (char)TCPOPT_WSCALE << (char)TCPOLEN_WSCALE << (char)u;
	    } else if (s + 6 <= end && memcmp(s, "sackOK", 6) == 0) {
		opt << (char)TCPOPT_SACK_PERMITTED << (char)TCPOLEN_SACK_PERMITTED;
		s += 6;
	    } else if (s + 10 < end && memcmp(s, "timestamp ", 10) == 0 && isdigit((unsigned char) s[10])) {
		s = cp_integer(s + 10, end, 0, &u);
		if (s + 1 < end && *s == ' ' && isdigit((unsigned char) s[1])) {
		    uint32_t u2;
		    s = cp_integer(s + 1, end, 0, &u2);
		    opt << (char)TCPOPT_TIMESTAMP << (char)TCPOLEN_TIMESTAMP;
		    append_net_uint32_t(opt, u);
		    append_net_uint32_t(opt, u2);
		}
	    } else if (s + 10 < end && memcmp(s, "sack sack ", 10) == 0 && isdigit((unsigned char) s[10])) {
		uint32_t nsack, u2;
		s = cp_integer(s + 10, end, 0, &nsack);
		opt << (char)TCPOPT_SACK << (char)(nsack * 8 + 2);
		while (s < end && *s == ' ')
		    s++;
		while (s + 1 < end && *s == '{' && isdigit((unsigned char) s[1])) {
		    s = cp_integer(s + 1, end, 0, &u);
		    if (s + 1 < end && *s == ':' && isdigit((unsigned char) s[1])) {
			s = cp_integer(s + 1, end, 0, &u2);
			if (s < end && *s == '}') {
			    s++;
			    if (record && !_absolute_seq) {
				u += record->init_seq[!rev];
				u2 += record->init_seq[!rev];
			    }
			    append_net_uint32_t(opt, u);
			    append_net_uint32_t(opt, u2);
			}
		    }
		}
		while (s < end && *s == ' ')
		    s++;
	    }
	    if (s < end && *s == ',')
		s++;
	    else if (s < end && *s != '>') { // not the option we thought
		opt.set_length(optlen1);
		while (s < end && *s != ',' && *s != '>')
		    s++;
	    }
	}
	if (s < end && *s == '>')
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

const char *
FromTcpdump::read_udp_line(WritablePacket *&, const char *s, const char *end, int *data_len)
{
    // first, check for '[bad hdr length]' '[udp sum ok]'
    const char *s2;
    if (s + 9 <= end && memcmp(s + 1, "[bad hdr", 8) == 0 && (s2 = find(s + 9, end, ']')) + 1 < end && s2[1] == ' ')
	s = s2 + 1;
    if (s + 14 <= end && memcmp(s + 1, "[udp sum ok] ", 13) == 0)
	s += 14;
    else if (s + 9 <= end && memcmp(s + 1, "[bad udp", 8) == 0 && (s2 = find(s + 9, end, ']')) + 1 < end && s2[1] == ' ')
	s = s2 + 2;

    // then check for 'udp LENGTH'
    if (s + 4 < end && s[0] == 'u' && s[1] == 'd' && s[2] == 'p' && s[3] == ' ' && isdigit((unsigned char) s[4])) {
	uint32_t dl;
	s = cp_integer(s + 4, end, 0, &dl);
	*data_len = dl;
    }

    return s;
}

Packet *
FromTcpdump::read_packet(ErrorHandler *errh)
{
    WritablePacket *q = Packet::make(0, (const unsigned char *)0, sizeof(click_ip) + sizeof(click_tcp), 20);
    if (!q) {
	_ff.error(errh, "out of memory!");
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
    iph->ip_ttl = 2;
    click_udp *udph = q->udp_header();

    String line;
    StringAccum payload;
    String ip_opt;
    String tcp_opt;

    while (1) {

	if (_ff.read_line(line, errh, true) <= 0) {
	    q->kill();
	    _dead = true;
	    return 0;
	}

	const char *s = line.data();
	const char *end = line.end();
	if (s >= end || s[0] == '#')
	    continue;
	else if (!isdigit((unsigned char) s[0]))
	    break;

	// first, read timestamp
	const char *s2 = find(s, end, ' ');
	if (!cp_time(line.substring(s, s2), &q->timestamp_anno()))
	    break;
	s = s2 + 1;

	// then, guess protocol
	iph->ip_p = 0;
	const char *colon = find(s, end, ':');
	for (colon++; colon < end && *colon == ' '; colon++)
	    /* nada */;
	if (colon < end) {
	    if (*colon == 'i')
		iph->ip_p = IP_PROTO_ICMP;
	    else if (*colon == 'u' || (*colon == '[' && colon + 1 < end && colon[1] == 'u'))
		iph->ip_p = IP_PROTO_UDP;
	    else if (*colon == '.' || (*colon >= 'A' && *colon <= 'Z'))
		iph->ip_p = IP_PROTO_TCP;
	}

	// then, read source IP address and port
	s2 = find(s, end, ' ');
	if (s2 == s || s2 + 2 >= end || s2[1] != '>' || s2[2] != ' ')
	    break;
	if (iph->ip_p == IP_PROTO_TCP || iph->ip_p == IP_PROTO_UDP) {
	    const char *sm = s2 - 1;
	    while (sm > s && *sm != '.' && *sm != ':')
		sm--;
	    if (!IPAddressArg().parse(line.substring(s, sm), iph->ip_src)
		|| !IPPortArg(iph->ip_p).parse(line.substring(sm + 1, s2), udph->uh_sport))
		break;
	    else
		udph->uh_sport = htons(udph->uh_sport);
	} else if (!IPAddressArg().parse(line.substring(s, s2), iph->ip_src))
	    break;
	s = s2 + 3;

	// then, read destination IP address and port
	s2 = find(s, end, ':');
	if (iph->ip_p == IP_PROTO_TCP || iph->ip_p == IP_PROTO_UDP) {
	    const char *sm = s2 - 1;
	    while (sm > s && *sm != '.' && *sm != ':')
		sm--;
	    if (!IPAddressArg().parse(line.substring(s, sm), iph->ip_dst)
		|| !IPPortArg(iph->ip_p).parse(line.substring(sm + 1, s2), udph->uh_dport))
		break;
	    else
		udph->uh_dport = htons(udph->uh_dport);
	} else if (!IPAddressArg().parse(line.substring(s, s2), iph->ip_dst))
	    break;

	// then, read protocol data
	int data_len = -1;
	if (iph->ip_p == IP_PROTO_TCP && IP_FIRSTFRAG(iph)) {
	    s = read_tcp_line(q, colon, end, &data_len);
	    iph = q->ip_header();
	} else if (iph->ip_p == IP_PROTO_UDP && IP_FIRSTFRAG(iph)) {
	    s = read_udp_line(q, colon, end, &data_len);
	    q->take(sizeof(click_tcp) - sizeof(click_udp));
	} else {
	    q->take(sizeof(click_tcp));
	    s = colon;
	}

	// parse IP stuff at the end of the line
	// TTL and ID
	s2 = end - 1;
	while (s2 > s) {
	    while (s2 > s && isspace((unsigned char) *s2))
		s2--;
	    if (s2 <= s || (*s2 != ')' && *s2 != ']'))
		break;
	    char opener = (*s2 == ')' ? '(' : '[');
	    const char *open = s2 - 1;
	    while (open >= s && *open != opener)
		open--;
	    const char *close = s2;
	    s2 = open - 1;
	    uint32_t u;

	    if (open >= s && open < close) {
		const char *item = open + 1;
		while (item < close) {
		    if (close - item >= 7 && memcmp(item, "tos 0x", 6) == 0) {
			item = cp_integer(item + 6, close, 16, &u);
			iph->ip_tos = u;
		    } else if (close - item >= 6 && memcmp(item, "ECT(", 4) == 0 && (item[4] == '0' || item[4] == '1') && item[5] == ')') {
			iph->ip_tos = (iph->ip_tos & ~IP_ECNMASK) | (item[4] == '0' ? IP_ECN_ECT1 : IP_ECN_ECT2);
			item += 6;
		    } else if (close - item >= 2 && item[0] == 'C' && item[1] == 'E') {
			iph->ip_tos = (iph->ip_tos & ~IP_ECNMASK) | IP_ECN_CE;
			item += 2;
		    } else if (close - item >= 2 && item[0] == 'D' && item[1] == 'F') {
			iph->ip_off |= htons(IP_DF);
			item += 2;
		    } else if (close - item >= 10 && memcmp(item, "frag ", 5) == 0 && isdigit((unsigned char) item[5])) {
			item = cp_integer(item + 5, close, 0, &u);
			iph->ip_id = htons(u);
			if (item > close - 2 || *item != ':' || !isdigit((unsigned char) item[1]))
			    break;
			item = cp_integer(item + 1, close, 0, &u);
			data_len = u;
			if (item > close - 2 || *item != '@' || !isdigit((unsigned char) item[1]))
			    break;
			item = cp_integer(item + 1, close, 0, &u);
			iph->ip_off = (iph->ip_off & htons(~IP_OFFMASK)) | htons(u);
			if (item < close && *item == '+')
			    iph->ip_off |= htons(IP_MF), item++;
		    } else if (close - item >= 5 && memcmp(item, "ttl ", 4) == 0 && isdigit((unsigned char) item[4])) {
			item = cp_integer(item + 4, close, 0, &u);
			iph->ip_ttl = u;
		    } else if (close - item >= 4 && memcmp(item, "id ", 3) == 0 && isdigit((unsigned char) item[3])) {
			item = cp_integer(item + 3, close, 0, &u);
			iph->ip_id = htons(u);
		    } else if (close - item >= 5 && memcmp(item, "len ", 4) == 0 && isdigit((unsigned char) item[4])) {
			item = cp_integer(item + 4, close, 0, &u);
			if (data_len < 0 || u == q->length() + data_len)
			    data_len = u - q->length();
			else if (iph->ip_p == IP_PROTO_TCP) {
			    // the discrepancy must be due to TCP options
			    int delta = u - (q->length() + data_len);
			    q->change_headroom_and_length(q->headroom(), u - data_len);
			    q->tcp_header()->th_off = (q->transport_length() >> 2);
			    if (delta > 0)
				q->end_data()[-delta] = TCPOPT_EOL;
			}
		    } else
			break;
		    while (item < close && (*item == ',' || isspace((unsigned char) *item)))
			item++;
		}
	    }
	}

	// set IP length
	if (data_len < 0)
	    data_len = 0;
	iph->ip_len = ntohs(q->length() + data_len);
	SET_EXTRA_LENGTH_ANNO(q, data_len);

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
	const char *s = line.begin();
	while (s < line.end() && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
	    s++;
	if (s != line.end()) {
	    _ff.error(errh, "packet parse error");
	    _format_complaint = true;
	}
    }
    if (q)
	q->kill();
    return 0;
}

bool
FromTcpdump::run_task(Task *)
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
	    _notifier.sleep();
	    return 0;
	}
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
FromTcpdump::read_handler(Element *e, void *thunk)
{
    FromTcpdump *fd = static_cast<FromTcpdump *>(e);
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
FromTcpdump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromTcpdump *fd = static_cast<FromTcpdump *>(e);
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
	      return errh->error("syntax error");
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
    add_read_handler("sampling_prob", read_handler, H_SAMPLING_PROB);
    add_read_handler("active", read_handler, H_ACTIVE, Handler::f_checkbox);
    add_write_handler("active", write_handler, H_ACTIVE);
    add_read_handler("encap", read_handler, H_ENCAP);
    add_write_handler("stop", write_handler, H_STOP, Handler::f_button);
    _ff.add_handlers(this);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
EXPORT_ELEMENT(FromTcpdump)
CLICK_ENDDECLS
