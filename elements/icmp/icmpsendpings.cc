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
#include <click/confparse.hh>
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
    : Element(0, 1), _limit(-1), _timer(this), _receiver(0)
{
    MOD_INC_USE_COUNT;
}

ICMPPingSource::~ICMPPingSource()
{
    MOD_DEC_USE_COUNT;
}

void
ICMPPingSource::notify_ninputs(int n)
{
    set_ninputs(n < 1 ? 0 : 1);
}

int
ICMPPingSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _icmp_id = 0;
    _interval = 1000;
    _data = String();
    _active = true;
    _verbose = true;
    if (cp_va_parse(conf, this, errh,
		    cpIPAddress, "source IP address", &_src,
		    cpIPAddress, "destination IP address", &_dst,
		    cpKeywords,
		    "INTERVAL", cpSecondsAsMilli, "time between pings (s)", &_interval,
		    "IDENTIFIER", cpUnsignedShort, "ICMP echo identifier", &_icmp_id,
		    "DATA", cpString, "payload", &_data,
		    "LIMIT", cpInteger, "total packet count", &_limit,
		    "ACTIVE", cpBool, "active?", &_active,
		    "VERBOSE", cpBool, "be verbose?", &_verbose,
		    cpEnd) < 0)
	return -1;
#ifndef __linux__
    _icmp_id = htons(_icmp_id);
#endif
    if (_interval == 0)
	errh->warning("INTERVAL so small that it is zero");
    return 0;
}

int
ICMPPingSource::initialize(ErrorHandler *errh)
{
    _count = 0;
    _timer.initialize(this);
    if (_limit != 0 && _active)
	_timer.schedule_after_ms(_interval);
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

void
ICMPPingSource::run_timer()
{
    WritablePacket *q = Packet::make(sizeof(click_ip) + sizeof(struct click_icmp_echo) + _data.length());
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
    click_gettimeofday(&q->timestamp_anno());

    if (_receiver)
	_receiver->send_timestamp[icp->icmp_sequence] = q->timestamp_anno();
    
    output(0).push(q);

    _count++;
    if (_count < _limit || _limit < 0)
	_timer.reschedule_after_ms(_interval);
}

void
ICMPPingSource::push(int, Packet *p)
{
    const click_ip *iph = p->ip_header();
    const click_icmp_echo *icmph = reinterpret_cast<const click_icmp_echo *>(p->icmp_header());
    if (iph && iph->ip_p == IP_PROTO_ICMP
	&& p->transport_length() >= (int)sizeof(click_icmp_echo)
	&& icmph->icmp_type == ICMP_ECHOREPLY
	&& icmph->icmp_identifier == _icmp_id) {
	struct timeval *send_ts = &_receiver->send_timestamp[icmph->icmp_sequence];
	
	if (!timerisset(send_ts))
	    /* error */;
	else {
	    if (send_ts->tv_usec >= 1000000) {
		_receiver->nduplicate++;
		send_ts->tv_usec -= 1000000;
	    }
	    
	    struct timeval diff = p->timestamp_anno() - *send_ts;
	    uint32_t diffval = (diff.tv_sec * 1000000) + diff.tv_usec;
	    if (diffval < _receiver->time_min || !_receiver->nreceived)
		_receiver->time_min = diffval;
	    if (diffval > _receiver->time_max || !_receiver->nreceived)
		_receiver->time_max = diffval;
	    _receiver->time_sum += diffval;
	    _receiver->time_sq_sum += ((counter_t)diffval) * diffval;

	    _receiver->nreceived++;
	    send_ts->tv_usec += 1000000;
	    
#ifdef __linux__
	    uint16_t readable_seq = icmph->icmp_sequence;
#else
	    uint16_t readable_seq = ntohs(icmph->icmp_sequence);
#endif
	    if (_verbose)
		click_chatter("%s: %d bytes from %s: icmp_seq=%u ttl=%u time=%d.%d ms", declaration().c_str(), ntohs(iph->ip_len) - (iph->ip_hl << 2) - sizeof(*icmph), IPAddress(iph->ip_dst).s().c_str(), readable_seq, iph->ip_ttl, (unsigned)(diffval/1000), (unsigned)(diffval % 1000));
	}
    }
    p->kill();
}

enum { H_ACTIVE, H_LIMIT, H_INTERVAL, H_RESET_COUNTS, H_COUNT, H_SUMMARY,
       H_RTT_MIN, H_RTT_AVG, H_RTT_MAX };

String
ICMPPingSource::read_handler(Element *e, void *thunk)
{
    ICMPPingSource *ps = static_cast<ICMPPingSource *>(e);
    ReceiverInfo *ri = ps->_receiver;
    switch ((uintptr_t)thunk) {
      case H_ACTIVE:
	return String(ps->_active) + "\n";
      case H_COUNT:
	return String(ps->_count) + "\n";
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
	return cp_unparse_microseconds(ri->time_min) + "\n";
      case H_RTT_AVG:
	return cp_unparse_microseconds(ri->time_sum / (ri->nreceived ? ri->nreceived : 1)) + "\n";
      case H_RTT_MAX:
	return cp_unparse_microseconds(ri->time_max) + "\n";
      default:
	return "";
    }
}

int
ICMPPingSource::write_handler(const String &str_in, Element *e, void *thunk, ErrorHandler *errh)
{
    String s = cp_uncomment(str_in);
    ICMPPingSource *ps = static_cast<ICMPPingSource *>(e);
    switch ((uintptr_t)thunk) {
      case H_ACTIVE:
	if (!cp_bool(s, &ps->_active))
	    return errh->error("'active' should be bool");
	if (ps->_active && !ps->_timer.scheduled())
	    ps->_timer.schedule_now();
	else if (!ps->_active)
	    ps->_timer.unschedule();
	return 0;
      case H_LIMIT:
	if (!cp_integer(s, &ps->_limit))
	    return errh->error("'limit' should be integer");
	if ((ps->_count < ps->_limit || ps->_limit < 0) && ps->_active && !ps->_timer.scheduled())
	    ps->_timer.schedule_after_ms(ps->_interval);
	return 0;
      case H_INTERVAL:
	if (!cp_seconds_as_milli(s, (uint32_t *)&ps->_interval))
	    return errh->error("'interval' should be an interval");
	return 0;
      case H_RESET_COUNTS:
	ps->_count = 0;
	if (ReceiverInfo *ri = ps->_receiver)
	    memset(ri, 0, sizeof(ReceiverInfo));
	if (ps->_count < ps->_limit && ps->_active && !ps->_timer.scheduled())
	    ps->_timer.schedule_after_ms(ps->_interval);
	return 0;
      default:
	return -1;
    }
}

void
ICMPPingSource::add_handlers()
{
    add_read_handler("active", read_handler, (void *)H_ACTIVE);
    add_write_handler("active", write_handler, (void *)H_ACTIVE);
    add_read_handler("count", read_handler, (void *)H_COUNT);
    add_write_handler("limit", write_handler, (void *)H_LIMIT);
    add_write_handler("interval", write_handler, (void *)H_INTERVAL);
    add_write_handler("reset_counts", write_handler, (void *)H_RESET_COUNTS);
    if (ninputs() > 0) {
	add_read_handler("summary", read_handler, (void *)H_SUMMARY);
	add_read_handler("rtt_min", read_handler, (void *)H_RTT_MIN);
	add_read_handler("rtt_avg", read_handler, (void *)H_RTT_AVG);
	add_read_handler("rtt_max", read_handler, (void *)H_RTT_MAX);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ICMPPingSource ICMPPingSource-ICMPSendPings)
