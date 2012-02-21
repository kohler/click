// -*- c-basic-offset: 4 -*-
/*
 * icmpsendpings.{cc,hh} -- Send ICMP ping packets.
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "icmpsendpings.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <click/packet_anno.hh>
#include <click/integers.hh>
#include <click/handlercall.hh>
#include <click/straccum.hh>
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/vmalloc.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
CLICK_DECLS

ICMPPingSource::ICMPPingSource()
    : _limit(-1), _timer(this), _receiver(0)
{
}

ICMPPingSource::~ICMPPingSource()
{
}

int
ICMPPingSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool has_interval;
    _icmp_id = 0;
    _interval = 1000;
    _data = String();
    _active = true;
    _verbose = true;
    _stop = false;
    if (Args(conf, this, errh)
	.read_mp("SRC", _src)
	.read_mp("DST", _dst)
	.read("INTERVAL", SecondsArg(3), _interval).read_status(has_interval)
	.read("IDENTIFIER", _icmp_id)
	.read("DATA", _data)
	.read("LIMIT", _limit)
	.read("ACTIVE", _active)
	.read("VERBOSE", _verbose)
	.read("STOP", _stop)
	.complete() < 0)
	return -1;
#ifndef __linux__
    _icmp_id = htons(_icmp_id);
#endif
    if (_interval == 0)
	errh->warning("INTERVAL so small that it is zero");
    if (output_is_pull(0) && has_interval)
	errh->warning("element is pull, INTERVAL parameter will be ignored");
    return 0;
}

int
ICMPPingSource::initialize(ErrorHandler *errh)
{
    _count = 0;
    _timer.initialize(this);
    if (_limit != 0 && _active && output_is_push(0))
	_timer.schedule_after_msec(_interval);
    if (ninputs() == 1) {
#if CLICK_LINUXMODULE
	_receiver = (ReceiverInfo *)vmalloc(sizeof(ReceiverInfo));
#else
	_receiver = new ReceiverInfo;
#endif
	if (!_receiver)
	    return errh->error("out of memory!");
	memset(_receiver, 0, sizeof(ReceiverInfo));
    }
    return 0;
}

void
ICMPPingSource::cleanup(CleanupStage)
{
    if (_receiver) {
	if (_verbose) {
	    PrefixErrorHandler perrh(ErrorHandler::default_handler(), declaration() + ": ");
	    perrh.message("%s", HandlerCall::call_read(this, "summary", &perrh).c_str());
	}
#if CLICK_LINUXMODULE
	vfree(_receiver);
#else
	delete _receiver;
#endif
    }
}

Packet*
ICMPPingSource::make_packet()
{
    WritablePacket *q = Packet::make(sizeof(click_ip) + sizeof(struct click_icmp_echo) + _data.length());
    if (!q)
	return 0;
    memset(q->data(), '\0', sizeof(click_ip) + sizeof(struct click_icmp_echo));
    memcpy(q->data() + sizeof(click_ip) + sizeof(struct click_icmp_echo), _data.data(), _data.length());

    click_ip *nip = reinterpret_cast<click_ip *>(q->data());
    nip->ip_v = 4;
    nip->ip_hl = sizeof(click_ip) >> 2;
    nip->ip_len = htons(q->length());
    uint16_t ip_id = (_count % 0xFFFF) + 1; // ensure ip_id != 0
    nip->ip_id = htons(ip_id);
    nip->ip_p = IP_PROTO_ICMP; /* icmp */
    nip->ip_ttl = 200;
    nip->ip_src = _src;
    nip->ip_dst = _dst;
    nip->ip_sum = click_in_cksum((unsigned char *)nip, sizeof(click_ip));

    click_icmp_echo *icp = (struct click_icmp_echo *) (nip + 1);
    icp->icmp_type = ICMP_ECHO;
    icp->icmp_code = 0;
    icp->icmp_identifier = _icmp_id;
#ifdef __linux__
    icp->icmp_sequence = ip_id;
#else
    icp->icmp_sequence = htons(ip_id);
#endif

    icp->icmp_cksum = click_in_cksum((const unsigned char *)icp, sizeof(click_icmp_sequenced) + _data.length());

    q->set_dst_ip_anno(IPAddress(_dst));
    q->set_ip_header(nip, sizeof(click_ip));
    q->timestamp_anno().assign_now();

    if (_receiver)
	_receiver->send_timestamp[icp->icmp_sequence] = q->timestamp_anno();

    return q;
}

void
ICMPPingSource::run_timer(Timer *)
{
    if (Packet *q = make_packet()) {
	output(0).push(q);
	_count++;
	if (_count < _limit || _limit < 0)
	    _timer.reschedule_after_msec(_interval);
	else if (_stop)
	    router()->please_stop_driver();
    }
}

Packet*
ICMPPingSource::pull(int)
{
    Packet *p = 0;
    if (_count < _limit || _limit < 0) {
	if ((p = make_packet()))
	    _count++;
    } else if (_stop)
	router()->please_stop_driver();
    return p;
}

void
ICMPPingSource::push(int, Packet *p)
{
    const click_ip *iph = p->ip_header();
    const click_icmp_echo *icmph = reinterpret_cast<const click_icmp_echo *>(p->icmp_header());
    if (p->has_network_header() && iph->ip_p == IP_PROTO_ICMP
	&& p->transport_length() >= (int)sizeof(click_icmp_echo)
	&& icmph->icmp_type == ICMP_ECHOREPLY
	&& icmph->icmp_identifier == _icmp_id) {
	Timestamp *send_ts = &_receiver->send_timestamp[icmph->icmp_sequence];

	if (!*send_ts)
	    /* error */;
	else {
	    if (send_ts->is_negative()) {
		_receiver->nduplicate++;
		*send_ts = -*send_ts;
	    }

	    uint32_t diffval;
	    if (p->timestamp_anno()) {
		Timestamp diff = p->timestamp_anno() - *send_ts;
		diffval = diff.usecval();
		if (diffval < _receiver->time_min || !_receiver->nreceived)
		    _receiver->time_min = diffval;
		if (diffval > _receiver->time_max || !_receiver->nreceived)
		    _receiver->time_max = diffval;
		_receiver->time_sum += diffval;
		_receiver->time_sq_sum += ((counter_t)diffval) * diffval;
	    } else
		diffval = 0;

	    _receiver->nreceived++;
	    *send_ts = -*send_ts;

#ifdef __linux__
	    uint16_t readable_seq = icmph->icmp_sequence;
#else
	    uint16_t readable_seq = ntohs(icmph->icmp_sequence);
#endif
	    if (_verbose)
		click_chatter("%s: %d bytes from %p{ip_ptr}: icmp_seq=%u ttl=%u time=%d.%03d ms", declaration().c_str(), ntohs(iph->ip_len) - (iph->ip_hl << 2) - sizeof(*icmph), &iph->ip_src, readable_seq, iph->ip_ttl, (unsigned)(diffval/1000), (unsigned)(diffval % 1000));
	}
    }
    p->kill();
}

enum { H_ACTIVE, H_LIMIT, H_INTERVAL, H_RESET_COUNTS, H_COUNT, H_SUMMARY,
       H_RTT_MIN, H_RTT_AVG, H_RTT_MAX, H_SRC, H_DST };

String
ICMPPingSource::read_handler(Element *e, void *thunk)
{
    ICMPPingSource *ps = static_cast<ICMPPingSource *>(e);
    ReceiverInfo *ri = ps->_receiver;
    switch ((uintptr_t)thunk) {
      case H_ACTIVE:
	return String(ps->_active);
      case H_COUNT:
	return String(ps->_count);
      case H_SRC:
	return IPAddress(ps->_src).unparse();
      case H_DST:
	return IPAddress(ps->_dst).unparse();
      case H_SUMMARY: {
	  StringAccum sa;
	  sa << ps->_count << " packets transmitted"
	     << ", " << (ri->nreceived - ri->nduplicate) << " received";
	  if (ri->nduplicate)
	      sa << ", +" << ri->nduplicate << " duplicates";
	  if (ps->_count)
	      sa << ", " << (int)(((ps->_count - ri->nreceived - ri->nduplicate) * 100) / ps->_count) << "% packet loss\n";
	  if (ri->nreceived) {
	      counter_t avg = ri->time_sum / ri->nreceived;
	      counter_t avg_sq = ri->time_sq_sum / ri->nreceived;
	      counter_t stdev = int_sqrt(avg_sq - avg * avg);
	      sa.snprintf(256, "rtt min/avg/max/mdev = %u.%03u/%u.%03u/%u.%03u/%u.%03u\n", ri->time_min / 1000, ri->time_min % 1000, (unsigned)(avg / 1000), (unsigned)(avg % 1000), ri->time_max / 1000, ri->time_max % 1000, (unsigned)(stdev / 1000), (unsigned)(stdev % 1000));
	  }
	  return sa.take_string();
	}
      case H_RTT_MIN:
	return cp_unparse_microseconds(ri->time_min);
      case H_RTT_AVG:
	return cp_unparse_microseconds(ri->time_sum / (ri->nreceived ? ri->nreceived : 1));
      case H_RTT_MAX:
	return cp_unparse_microseconds(ri->time_max);
      default:
	return "";
    }
}

int
ICMPPingSource::write_handler(const String &s, Element *e, void *thunk, ErrorHandler *errh)
{
    ICMPPingSource *ps = static_cast<ICMPPingSource *>(e);
    switch ((uintptr_t)thunk) {
    case H_ACTIVE:
	if (!BoolArg().parse(s, ps->_active))
	    return errh->error("type mismatch");
	if (ps->_active && !ps->_timer.scheduled() && ps->output_is_push(0))
	    ps->_timer.schedule_now();
	else if (!ps->_active)
	    ps->_timer.unschedule();
	return 0;
    case H_SRC:
	if (!IPAddressArg().parse(s, ps->_src))
	    return errh->error("syntax error");
	return 0;
    case H_DST:
	if (!IPAddressArg().parse(s, ps->_dst))
	    return errh->error("syntax error");
	return 0;
      case H_LIMIT:
	  if (!IntArg().parse(s, ps->_limit))
	    return errh->error("'limit' should be integer");
	if ((ps->_count < ps->_limit || ps->_limit < 0) && ps->_active && !ps->_timer.scheduled() && ps->output_is_push(0))
	    ps->_timer.schedule_after_msec(ps->_interval);
	return 0;
      case H_INTERVAL:
	  if (!SecondsArg(3).parse_saturating(s, ps->_interval))
	    return errh->error("'interval' should be an interval");
	return 0;
      case H_RESET_COUNTS:
	ps->_count = 0;
	if (ReceiverInfo *ri = ps->_receiver)
	    memset(ri, 0, sizeof(ReceiverInfo));
	if (ps->_count < ps->_limit && ps->_active && !ps->_timer.scheduled() && ps->output_is_push(0))
	    ps->_timer.schedule_after_msec(ps->_interval);
	return 0;
      default:
	return -1;
    }
}

void
ICMPPingSource::add_handlers()
{
    add_read_handler("active", read_handler, H_ACTIVE, Handler::CHECKBOX);
    add_write_handler("active", write_handler, H_ACTIVE);
    add_read_handler("src", read_handler, H_SRC, Handler::CALM);
    add_write_handler("src", write_handler, H_SRC);
    add_read_handler("dst", read_handler, H_DST, Handler::CALM);
    add_write_handler("dst", write_handler, H_DST);
    add_read_handler("count", read_handler, H_COUNT);
    add_write_handler("limit", write_handler, H_LIMIT, Handler::CALM);
    add_write_handler("interval", write_handler, H_INTERVAL);
    add_write_handler("reset_counts", write_handler, H_RESET_COUNTS, Handler::BUTTON);
    if (ninputs() > 0) {
	add_read_handler("summary", read_handler, H_SUMMARY);
	add_read_handler("rtt_min", read_handler, H_RTT_MIN);
	add_read_handler("rtt_avg", read_handler, H_RTT_AVG);
	add_read_handler("rtt_max", read_handler, H_RTT_MAX);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ICMPPingSource ICMPPingSource-ICMPSendPings)
