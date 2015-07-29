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

#include "fromcapdump.hh"
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/packet_anno.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
CLICK_DECLS

FromCapDump::FromCapDump()
    : _flowid(IPAddress(htonl(0x01000001)), htons(1), IPAddress(htonl(0x02000002)), htons(2)),
      _flowid_is_rcv(false), _aggregate(1), _p2s_map(1024, NO_SEQNO),
      _task(this)
{
    _p2s_map[0] = 0;
    _ff.set_landmark_pattern("%f:%l");
}

FromCapDump::~FromCapDump()
{
}

void *
FromCapDump::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0 && !output_is_push(0))
	return static_cast<Notifier *>(&_notifier);
    else
	return Element::cast(n);
}

int
FromCapDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false, active = true, zero = true, checksum = false;
    _sampling_prob = (1 << SAMPLING_SHIFT);

    if (Args(conf, this, errh)
	.read_mp("FILENAME", FilenameArg(), _ff.filename())
	.read("STOP", stop)
	.read("ACTIVE", active)
	.read("ZERO", zero)
	.read("CHECKSUM", checksum)
	.read("AGGREGATE", _aggregate)
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
    return 0;
}

int
FromCapDump::initialize(ErrorHandler *errh)
{
    // make sure notifier is initialized
    if (!output_is_push(0))
	_notifier.initialize(Notifier::EMPTY_NOTIFIER, router());

    if (_ff.initialize(errh) < 0)
	return -1;

    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    return 0;
}

void
FromCapDump::cleanup(CleanupStage)
{
    _ff.cleanup();
}

uint32_t
FromCapDump::packno2seqno(uint32_t p)
{
    while (p + 1 >= (uint32_t) _p2s_map.size())
	_p2s_map.resize(_p2s_map.size() * 2, NO_SEQNO);
    if (_p2s_map[p] == NO_SEQNO) { // backpatch
	uint32_t p2 = p;
	while (_p2s_map[p2 - 1] == NO_SEQNO)
	    p2--;
	int packet_len = (p2 > 1 ? _p2s_map[p2 - 1] - _p2s_map[p2 - 2] : 1460);
	for (; p2 <= p; p2++)
	    _p2s_map[p2] = _p2s_map[p2 - 1] + packet_len;
    }
    return _p2s_map[p];
}

uint32_t
FromCapDump::packno2seqno(uint32_t p, int len)
{
    uint32_t seqno = packno2seqno(p);
    if (!(_p2s_map[p + 1] == NO_SEQNO || _p2s_map[p + 1] == seqno + len))
	click_chatter("   %s >> %u %u %u", _ff.landmark().c_str(), p+1, _p2s_map[p+1], seqno + len);
    assert(_p2s_map[p + 1] == NO_SEQNO || _p2s_map[p + 1] == seqno + len);
    _p2s_map[p + 1] = seqno + len;
    return seqno;
}

static void
set_checksums(WritablePacket *q, click_ip *iph)
{
    assert(iph == q->ip_header());

    iph->ip_sum = 0;
    iph->ip_sum = click_in_cksum((uint8_t *)iph, iph->ip_hl << 2);

    click_tcp *tcph = q->tcp_header();
    tcph->th_sum = 0;
    unsigned csum = click_in_cksum((uint8_t *)tcph, q->transport_length());
    tcph->th_sum = click_in_cksum_pseudohdr(csum, iph, q->transport_length());
}

Packet *
FromCapDump::read_packet(ErrorHandler *errh)
{
    // ensure puts will succeed
    WritablePacket *q = Packet::make(0, (const unsigned char *)0, sizeof(click_ip) + sizeof(click_tcp), 60);
    if (!q) {
	_ff.error(errh, strerror(ENOMEM));
	return 0;
    }
    if (_zero)
	memset(q->data(), 0, q->length());
    q->set_network_header(q->data(), sizeof(click_ip));
    click_ip *iph = q->ip_header();
    iph->ip_v = 4;
    iph->ip_hl = sizeof(click_ip) >> 2;
    iph->ip_p = IP_PROTO_TCP;
    iph->ip_off = 0;
    click_tcp *tcph = q->tcp_header();
    tcph->th_win = htons(65535);

    String line;

    while (1) {

	if (_ff.read_line(line, errh, true) <= 0) {
	    q->kill();
	    return 0;
	}

	const char *data = line.begin();
	const char *end = line.end();
	uint32_t u1, u2;

	if (data + 3 >= end || data[0] == '!' || data[0] == '#'
	    || !isspace((unsigned char) data[3]))
	    continue;

	if ((data[0] != 'D' || data[1] != 'A' || data[2] != 'T')
	    && (data[0] != 'A' || data[1] != 'C' || data[2] != 'K')
	    && (data[0] != 'S' || data[1] != 'Y' || data[2] != 'N')) {
	    // swap flowid if allowed
	    if ((data[0] == 'R' && data[1] == 'C' && data[2] == 'V')
		|| (data[0] == 'S' && data[1] == 'N' && data[2] == 'D')) {
		if (_flowid_is_rcv != (data[0] == 'R')) {
		    _flowid_is_rcv = !_flowid_is_rcv;
		    _flowid = _flowid.reverse();
		}
	    }
	    continue;
	}

	char packet_type = data[0];
	tcph->th_flags = (data[0] == 'S' ? TH_SYN : 0);
	data = cp_skip_space(data + 4, end);

	// read direction
	if (data[0] != '<' && data[0] != '>')
	    continue;
	SET_PAINT_ANNO(q, (data[0] == '>') == _flowid_is_rcv);
	data = cp_skip_space(data + 1, end);

	// read timestamp
	const char *next = cp_integer(data, end, 10, &u1);
	if (next == data)
	    continue;
	data = next;
	u2 = 0;
	if (data + 1 < end && *data == '.') {
	    int digit = 0;
	    for (data++; digit < 6 && data < end && isdigit((unsigned char) *data); digit++, data++)
		u2 = (u2 * 10) + *data - '0';
	    for (; digit < 6; digit++)
		u2 = (u2 * 10);
	    for (; data < end && isdigit((unsigned char) *data); data++)
		/* nada */;
	}
	q->set_timestamp_anno(Timestamp(u1, u2));
	data = cp_skip_space(data, end);

	// read sequence numbers and lengths
	uint32_t uniqno, seqno, ip_len, payload_len;
	data = cp_integer(data, end, 10, &uniqno);
	data = cp_integer(cp_skip_space(data, end), end, 10, &seqno);
	data = cp_integer(cp_skip_space(data, end), end, 10, &ip_len);
	data = cp_skip_space(data, end);
	next = cp_integer(data, end, 10, &payload_len);
	if (data == next)
	    continue;

	// extra stuff, if any
      parse_annotations:
	data = cp_skip_space(next, end);
	if (data + 6 < end && data[0] == 'D' && data[1] == 'S'
	    && data[2] == 'A' && data[3] == 'C' && data[4] == 'K'
	    && data[5] == ':'
	    && (next = cp_integer(data + 6, end, 10, &u1))) {
	    q = q->put(12);
	    uint8_t *opt = q->transport_header() + sizeof(click_tcp);
	    *opt++ = TCPOPT_NOP;
	    *opt++ = TCPOPT_NOP;
	    *opt++ = TCPOPT_SACK;
	    *opt++ = 10;
	    *(reinterpret_cast<uint32_t *>(opt)) = htonl(packno2seqno(u1));
	    *(reinterpret_cast<uint32_t *>(opt + 4)) = htonl(packno2seqno(u1 + 1));
	    goto parse_annotations;

	} else if (data + 2 < end && data[0] == 'F' && data[1] == 'I'
		   && data[2] == 'N') {
	    tcph->th_flags |= TH_FIN;
	    next = data + 3;
	    goto parse_annotations;

	} else if (data + 1 < end && data[0] == 'F' && data[1] == 'R') {
	    next = data + 2;
	    goto parse_annotations;

	} else if (data + 2 < end && data[0] == 'R' && data[1] == 'T'
		   && data[2] == 'O') {
	    next = data + 3;
	    goto parse_annotations;

	} else if (data + 4 < end && data[0] == 'S' && data[1] == 'A'
		   && data[2] == 'C' && data[3] == 'K' && data[4] == ':') {
	    q = q->put(40);
	    uint8_t *opt = q->transport_header() + sizeof(click_tcp);
	    *opt++ = TCPOPT_NOP;
	    *opt++ = TCPOPT_NOP;
	    *opt++ = TCPOPT_SACK;
	    opt++;

	    data += 4;
	    while (data < end && *data == ':') {
		next = cp_integer(data + 1, end, 10, &u1);
		if (next != data + 1 && next + 1 < end && next[0] == '-'
		    && isdigit((unsigned char) next[1])) {
		    data = cp_integer(next + 1, end, 10, &u2);
		    *(reinterpret_cast<uint32_t *>(opt)) = htonl(packno2seqno(u1));
		    *(reinterpret_cast<uint32_t *>(opt + 4)) = htonl(packno2seqno(u2 + 1));
		    opt += 8;
		} else
		    break;
	    }
	    q->transport_header()[sizeof(click_tcp) + 3] = opt - (q->transport_header() + sizeof(click_tcp) + 2);
	    q->take(q->end_data() - opt);
	    next = data;
	    goto parse_annotations;

	} else if (data + 7 < end && data[0] == 'S' && data[1] == 'A'
		   && data[2] == 'C' && data[3] == 'K' && data[4] == '_'
		   && data[5] == 'N' && data[6] == 'E' && data[7] == 'W') {
	    next = data + 8;
	    goto parse_annotations;

	} else if (data + 9 < end && data[0] == 'S' && data[1] == 'A'
		   && data[2] == 'C' && data[3] == 'K' && data[4] == '_'
		   && data[5] == 'R' && data[6] == 'E' && data[7] == 'X'
		   && data[8] == 'M' && data[9] == 'T') {
	    next = data + 10;
	    goto parse_annotations;

	} else if (data + 2 < end && data[0] == 'T' && data[1] == 'I'
		   && data[2] == 'M') {
	    next = data + 3;
	    goto parse_annotations;
	}

	if (packet_type == 'D') {
	    tcph->th_seq = htonl(packno2seqno(seqno, payload_len + (tcph->th_flags & TH_SYN ? 1 : 0) + (tcph->th_flags & TH_FIN ? 1 : 0)));
	    tcph->th_ack = htonl(1);
	    tcph->th_flags |= TH_ACK;
	} else if (packet_type == 'A') {
	    tcph->th_seq = htonl(1);
	    tcph->th_ack = htonl(packno2seqno(seqno + 1));
	    tcph->th_flags |= TH_ACK;
	} else {
	    tcph->th_seq = htonl(0);
	    if (_flowid_is_rcv != (PAINT_ANNO(q) != 0)) {
		tcph->th_ack = htonl(1);
		tcph->th_flags |= TH_ACK;
	    } else
		tcph->th_ack = htonl(0);
	    (void) packno2seqno(seqno, payload_len + (tcph->th_flags & TH_SYN ? 1 : 0) + (tcph->th_flags & TH_FIN ? 1 : 0));
	}
	SET_PACKET_NUMBER_ANNO(q, uniqno);
	SET_SEQUENCE_NUMBER_ANNO(q, seqno);

	tcph->th_off = (q->end_data() - q->transport_header()) >> 2;

	// set IP length
	iph->ip_len = ntohs(q->length() + payload_len);
	SET_EXTRA_LENGTH_ANNO(q, payload_len);

	// set data from flow ID
	IPFlowID flowid = (PAINT_ANNO(q) & 1 ? _flowid.reverse() : _flowid);
	iph->ip_src = flowid.saddr();
	iph->ip_dst = flowid.daddr();
	tcph->th_sport = flowid.sport();
	tcph->th_dport = flowid.dport();
	SET_AGGREGATE_ANNO(q, _aggregate);

	// set checksum
	if (_checksum)
	    set_checksums(q, iph);

	return q;
    }

    if (q)
	q->kill();
    return 0;
}

bool
FromCapDump::run_task(Task *)
{
    if (!_active)
	return false;
    Packet *p;

    while (1) {
	p = read_packet(0);
	if (!p) {
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
FromCapDump::pull(int)
{
    if (!_active)
	return 0;
    Packet *p;

    while (1) {
	p = read_packet(0);
	if (!p) {
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
FromCapDump::read_handler(Element *e, void *thunk)
{
    FromCapDump *fd = static_cast<FromCapDump *>(e);
    switch ((intptr_t)thunk) {
      case H_SAMPLING_PROB:
	return cp_unparse_real2(fd->_sampling_prob, SAMPLING_SHIFT);
      case H_ENCAP:
	return "IP";
      default:
	return "<error>";
    }
}

int
FromCapDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromCapDump *fd = static_cast<FromCapDump *>(e);
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
FromCapDump::add_handlers()
{
    add_read_handler("sampling_prob", read_handler, H_SAMPLING_PROB);
    add_data_handlers("active", Handler::f_read | Handler::f_checkbox, &_active);
    add_write_handler("active", write_handler, H_ACTIVE);
    add_read_handler("encap", read_handler, H_ENCAP);
    add_write_handler("stop", write_handler, H_STOP);
    _ff.add_handlers(this);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromCapDump)
CLICK_ENDDECLS
