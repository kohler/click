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
#include <click/args.hh>
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
    : _packet(0), _work_packet(0), _timer(this), _task(this)
{
    _ff.set_landmark_pattern("%f:%l");
}

FromNetFlowSummaryDump::~FromNetFlowSummaryDump()
{
}

void *
FromNetFlowSummaryDump::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0 && !output_is_push(0))
	return static_cast<Notifier *>(&_notifier);
    else
	return Element::cast(n);
}

int
FromNetFlowSummaryDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool stop = false;
    _active = _zero = true;
    _multipacket = _timing = false;
    String link = "input";

    if (Args(conf, this, errh)
	.read_mp("FILENAME", FilenameArg(), _ff.filename())
	.read("STOP", stop)
	.read("ACTIVE", _active)
	.read("ZERO", _zero)
	.read("MULTIPACKET", _multipacket)
	.read("LINK", WordArg(), link)
	.read("TIMING", _timing)
	.complete() < 0)
	return -1;

    _stop = stop;
    link = link.lower();
    if (link == "input")
	_link = 0;
    else if (link == "output")
	_link = 1;
    else if (link == "both")
	_link = 2;
    else
	return errh->error("'LINK' should be 'input', 'output', or 'both'");
    return 0;
}

int
FromNetFlowSummaryDump::initialize(ErrorHandler *errh)
{
    if (!output_is_push(0))
	_notifier.initialize(Notifier::EMPTY_NOTIFIER, router());

    if (_ff.initialize(errh) < 0)
	return -1;

    String line;
    if (_ff.peek_line(line, errh, true) < 0)
	return -1;

    _format_complaint = false;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);
    _timer.initialize(this);
    _have_timing = false;
    return 0;
}

void
FromNetFlowSummaryDump::cleanup(CleanupStage)
{
    _ff.cleanup();
    if (_packet)
	_packet->kill();
    if (_work_packet)
	_work_packet->kill();
    _packet = _work_packet = 0;
}

Packet *
FromNetFlowSummaryDump::read_packet(ErrorHandler *errh)
{
    WritablePacket *q = Packet::make((const char *)0, sizeof(click_ip) + sizeof(click_tcp));
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
    iph->ip_off = 0;

    String line;
    String words[15];
    uint32_t j;

    while (1) {

	if (_ff.read_line(line, errh, true) <= 0) {
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
	// 3 - input interface
	// 4 - output interface
	// 5 - # packets
	// 6 - # bytes
	// 7 - start timestamp sec
	// 8 - end timestamp sec
	// 9 - source port
	// 10 - dest port
	// 13 - protocol
	// 14 - TOS bits

	int ok = 0;

	// annotations
	if (IntArg().parse(words[7], j))
	    SET_FIRST_TIMESTAMP_ANNO(q, Timestamp(j, 0)), ok++;
	if (IntArg().parse(words[8], j)) {
	    if (j)
		q->timestamp_anno().assign(j, 0);
	    else
		q->timestamp_anno() = FIRST_TIMESTAMP_ANNO(q);
	    ok++;
	}
	if (IntArg().parse(words[5], j))
	    SET_EXTRA_PACKETS_ANNO(q, j - 1), ok++;
	uint32_t byte_count = 0;
	if (IntArg().parse(words[6], byte_count))
	    ok++;
	uint32_t input = 0, output = 0;
	if ((_link == 1 || IntArg().parse(words[3], input))
	    && (_link == 0 || IntArg().parse(words[4], output))) {
	    ok++;
	    uint32_t m = (_link == 2 ? 15 : 255);
	    input = (input < m ? input : m) << (_link == 2 ? 4 : 0);
	    output = (output < m ? output : m);
	    SET_PAINT_ANNO(q, input | output);
	}

	// IP header
	ok += IPAddressArg().parse(words[0], iph->ip_src);
	ok += IPAddressArg().parse(words[1], iph->ip_dst);
	if (IntArg().parse(words[13], j) && j <= 0xFF)
	    iph->ip_p = j, ok++;
	if (IntArg().parse(words[14], j) && j <= 0xFF)
	    iph->ip_tos = j, ok++;

	// TCP header
	if (IntArg().parse(words[9], j) && j <= 0xFFFF)
	    q->udp_header()->uh_sport = htons(j), ok++;
	if (IntArg().parse(words[10], j) && j <= 0xFFFF)
	    q->udp_header()->uh_dport = htons(j), ok++;
	if (IntArg().parse(words[12], j) && j <= 0xFF)
	    q->tcp_header()->th_flags = j, ok++;

	if (ok < 10)
	    break;

	// set TCP offset to a reasonable value; possibly reduce packet length
	int ip_len = (byte_count <= 65535 ? byte_count : 65535);
	iph->ip_len = htons(ip_len);
	if (iph->ip_p == IP_PROTO_TCP) {
	    q->tcp_header()->th_off = sizeof(click_tcp) >> 2;
	    q->tcp_header()->th_flags2 = 0;
	} else if (iph->ip_p == IP_PROTO_UDP) {
	    q->udp_header()->uh_ulen = htons(ip_len - sizeof(click_ip));
	    q->take(sizeof(click_tcp) - sizeof(click_udp));
	} else
	    q->take(sizeof(click_tcp));
	SET_EXTRA_LENGTH_ANNO(q, byte_count - q->length());
	return q;
    }

    // bad format if we get here
    if (!_format_complaint) {
	_ff.error(errh, "bad format");
	_format_complaint = true;
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
FromNetFlowSummaryDump::handle_multipacket(Packet *p)
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
	// prepare IP lengths for _multipacket_length
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

Packet *
FromNetFlowSummaryDump::next_packet()
{
    if (!_packet) {
      retry:
	_packet = (_work_packet ? _work_packet : read_packet(0));
	if (_packet && _multipacket) {
	    if (!(_packet = handle_multipacket(_packet)))
		goto retry;
	}
    }
    return _packet;
}

void
FromNetFlowSummaryDump::run_timer(Timer *)
{
    if (_active) {
	if (output_is_push(0))
	    _task.reschedule();
	else
	    _notifier.wake();
    }
}

bool
FromNetFlowSummaryDump::check_timing(Packet *p)
{
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
	return false;
    }
    return true;
}

bool
FromNetFlowSummaryDump::run_task(Task *)
{
    if (!_active)
	return false;

    Packet *p = next_packet();
    _packet = 0;
    if (!p) {
	if (_stop)
	    router()->please_stop_driver();
	return false;
    } else if (_timing && !check_timing(p))
	return false;
<<<<<<< HEAD
=======
    _packet = 0;
>>>>>>> kohler/master
    output(0).push(p);
    _task.fast_reschedule();
    return true;
}

Packet *
FromNetFlowSummaryDump::pull(int)
{
    if (!_active) {
	_notifier.sleep();
	return 0;
    }

    Packet *p = next_packet();
    _packet = 0;
    if (!p && _stop)
	router()->please_stop_driver();
    else if (p && _timing && !check_timing(p))
	return 0;
    _notifier.set_active(p != 0, true);
    _packet = 0;
    return p;
}


enum { H_ACTIVE, H_ENCAP };

String
FromNetFlowSummaryDump::read_handler(Element *e, void *thunk)
{
    FromNetFlowSummaryDump *fd = static_cast<FromNetFlowSummaryDump *>(e);
    switch ((intptr_t)thunk) {
      case H_ACTIVE:
	return BoolArg::unparse(fd->_active);
      case H_ENCAP:
	return "IP";
      default:
	return "<error>";
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
	  if (BoolArg().parse(s, active)) {
	      fd->_active = active;
	      if (fd->output_is_push(0)) {
		  if (active && !fd->_task.scheduled())
		      fd->_task.reschedule();
	      } else
		  fd->_notifier.set_active(active, true);
	      return 0;
	  } else
	      return errh->error("type mismatch");
      }
      default:
	return -EINVAL;
    }
}

void
FromNetFlowSummaryDump::add_handlers()
{
    add_read_handler("active", read_handler, H_ACTIVE, Handler::f_checkbox);
    add_write_handler("active", write_handler, H_ACTIVE);
    add_read_handler("encap", read_handler, H_ENCAP);
    _ff.add_handlers(this);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromNetFlowSummaryDump)
