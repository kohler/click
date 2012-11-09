// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipflowrawsockets.{cc,hh} -- creates sockets for TCP/UDP flows
 * Mark Huang <mlhuang@cs.princeton.edu>
 *
 * Copyright (c) 2004  The Trustees of Princeton University (Trustees).
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
#include "ipflowrawsockets.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <clicknet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include <click/straccum.hh>

extern "C" {
#include <pcap.h>
}

#include "fakepcap.hh"
#include <clicknet/udp.h>

CLICK_DECLS

IPFlowRawSockets::Flow::Flow(const Packet *p)
    : _next(0),
      _flowid(p), _ip_p(p->ip_header()->ip_p),
      _aggregate(AGGREGATE_ANNO(p)),
      _wd(-1), _rd(-1), _pcap(0)
{
    if (PAINT_ANNO(p) & 1)	// reverse _flowid
	_flowid = _flowid.reverse();

    // sanity checks
    assert(_aggregate && (_ip_p == IP_PROTO_TCP || _ip_p == IP_PROTO_UDP));
}

IPFlowRawSockets::Flow::~Flow()
{
    if (_wd >= 0)
	close(_wd);
    if (_pcap)
	pcap_close(_pcap);
}

int
IPFlowRawSockets::Flow::initialize(ErrorHandler *errh, int snaplen, bool usepcap)
{
    struct sockaddr_in sin;

    // open raw IP socket
    _wd = socket(PF_INET, SOCK_RAW, _ip_p);
    if (_wd < 0)
	return errh->error("socket: %s", strerror(errno));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = PF_INET;
    sin.sin_port = _flowid.sport();
    sin.sin_addr = IPAddress().in_addr();

    // bind to source port
    if (bind(_wd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	return errh->error("bind: %s", strerror(errno));

    // include IP header in messages
    int one = 1;
    if (setsockopt(_wd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0)
	return errh->error("setsockopt: %s", strerror(errno));

    // nonblocking I/O and close-on-exec for the socket
    fcntl(_wd, F_SETFL, O_NONBLOCK);
    fcntl(_wd, F_SETFD, FD_CLOEXEC);

    // don't use libpcap to capture
    if (!usepcap) {
	_rd = _wd;
	return 0;
    }

    char ebuf[PCAP_ERRBUF_SIZE];
    _pcap = pcap_open_live("any", snaplen, false,
			   1,     /* timeout: don't wait for packets */
			   ebuf);
    // Note: pcap error buffer will contain the interface name
    if (!_pcap) {
	errh->warning("pcap_open_live: %s", ebuf);
	_rd = _wd;
	return 0;
    }

    // nonblocking I/O on the packet socket so we can poll
    _rd = pcap_fileno(_pcap);
    fcntl(_rd, F_SETFL, O_NONBLOCK);

#ifdef BIOCSSEESENT
    {
	int accept = 0;
	if (ioctl(_rd, BIOCSSEESENT, &accept) != 0)
	    return errh->error("ioctl: %s", strerror(errno));
    }
#endif

    // build the BPF filter
    StringAccum ss;
    ss << "ip src host ";
    ss << _flowid.daddr().unparse().c_str();
    ss << " and ";
    ss << (_ip_p == IP_PROTO_TCP ? "tcp" : "udp");
    ss << " src port ";
    ss << ntohs(_flowid.dport());
    ss << " and ";
    ss << (_ip_p == IP_PROTO_TCP ? "tcp" : "udp");
    ss << " dst port ";
    ss << ntohs(_flowid.sport());

    // compile the BPF filter
    struct bpf_program fcode;
    if (pcap_compile(_pcap, &fcode, (char *)ss.c_str(), 0, 0) < 0)
	return errh->error("pcap_compile: %s", pcap_geterr(_pcap));
    if (pcap_setfilter(_pcap, &fcode) < 0)
	return errh->error("pcap_setfilter: %s", pcap_geterr(_pcap));

    _datalink = pcap_datalink(_pcap);
    if (!fake_pcap_dlt_force_ipable(_datalink))
	errh->warning("strange data link type %d", _datalink);

    return 0;
}

void
IPFlowRawSockets::Flow::send_pkt(Packet *p, ErrorHandler *errh)
{
    struct sockaddr_in sin;

    sin.sin_family = PF_INET;
    sin.sin_port = _flowid.dport();
    sin.sin_addr = _flowid.daddr().in_addr();

    // write data to socket
    int w = 0;
    while (p->length()) {
	w = sendto(_wd, p->data(), p->length(), 0, (const struct sockaddr*)&sin, sizeof(sin));
	if (w < 0 && errno != EINTR)
	    break;
	p->pull(w);
    }
    if (w < 0 && errno != EAGAIN)
	errh->error("sendto: %s", strerror(errno));

    p->kill();
}

IPFlowRawSockets::IPFlowRawSockets()
    : _nnoagg(0), _nagg(0), _agg_notifier(0), _task(this),
      _gc_timer(gc_hook, this), _headroom(Packet::default_headroom)
{
    for (int i = 0; i < NFLOWMAP; i++)
	_flowmap[i] = 0;
}

IPFlowRawSockets::~IPFlowRawSockets()
{
}

int
IPFlowRawSockets::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e = 0;

    _snaplen = 2046;
    _usepcap = true;
    if (Args(conf, this, errh)
	.read("NOTIFIER", e)
	.read("SNAPLEN", _snaplen)
	.read("PCAP", _usepcap)
	.read("HEADROOM", _headroom)
	.complete() < 0)
	return -1;

    if (e && !(_agg_notifier = (AggregateNotifier *)e->cast("AggregateNotifier")))
	return errh->error("%s is not an AggregateNotifier", e->name().c_str());

    return 0;
}


void
IPFlowRawSockets::end_flow(Flow *f, ErrorHandler *)
{
    remove_select(f->rd(), SELECT_READ);
    _flows[f->rd()] = 0;
    delete f;
    _nflows--;
}

void
IPFlowRawSockets::cleanup(CleanupStage)
{
    ErrorHandler *errh = ErrorHandler::default_handler();
    for (int i = 0; i < NFLOWMAP; i++)
	while (Flow *f = _flowmap[i]) {
	    _flowmap[i] = f->next();
	    end_flow(f, errh);
	}
    if (_nnoagg > 0 && _nagg == 0)
	errh->lwarning(declaration(), "saw no packets with aggregate annotations");
}

int
IPFlowRawSockets::initialize(ErrorHandler *errh)
{
    if (input_is_pull(0)) {
	ScheduleInfo::join_scheduler(this, &_task, errh);
	_signal = Notifier::upstream_empty_signal(this, 0, &_task);
    }
    if (_agg_notifier)
	_agg_notifier->add_listener(this);
    _gc_timer.initialize(this);
    return 0;
}

IPFlowRawSockets::Flow *
IPFlowRawSockets::find_aggregate(uint32_t agg, const Packet *p)
{
    if (agg == 0)
	return 0;

    int bucket = (agg & (NFLOWMAP - 1));
    Flow *prev = 0, *f = _flowmap[bucket];
    while (f && f->aggregate() != agg) {
	prev = f;
	f = f->next();
    }

    if (f)
	/* nada */;
    else if (p && (f = new Flow(p))) {
	if (f->initialize(ErrorHandler::default_handler(), _snaplen, _usepcap)) {
	    delete f;
	    return 0;
	}
	while (f->rd() >= _flows.size())
	    _flows.push_back(0);
	_flows[f->rd()] = f;
	add_select(f->rd(), SELECT_READ);
	prev = f;
	_nflows++;
    } else
	return 0;

    if (prev) {
	prev->set_next(f->next());
	f->set_next(_flowmap[bucket]);
	_flowmap[bucket] = f;
    }

    return f;
}

void
IPFlowRawSockets::push(int, Packet *p)
{
    if (Flow *f = find_aggregate(AGGREGATE_ANNO(p), p)) {
	_nagg++;
	f->send_pkt(p, ErrorHandler::default_handler());
    } else
	_nnoagg++;
}

CLICK_ENDDECLS
extern "C" {
void
IPFlowRawSockets_get_packet(u_char* clientdata,
			 const struct pcap_pkthdr* pkthdr,
			 const u_char* data)
{
    WritablePacket *p = (WritablePacket *) clientdata;
    int length = pkthdr->caplen;

    memcpy(p->data(), data, length);
    p = p->put(length);
    assert(p);

    // set annotations
    p->timestamp_anno().assign_usec(pkthdr->ts.tv_sec, pkthdr->ts.tv_usec);
    SET_EXTRA_LENGTH_ANNO(p, pkthdr->len - length);
}
}
CLICK_DECLS

void
IPFlowRawSockets::selected(int fd, int)
{
    ErrorHandler *errh = ErrorHandler::default_handler();
    WritablePacket *p;
    int len;

    assert(fd < _flows.size());
    Flow *f = _flows[fd];
    assert(f);

    // Read and push() at most one packet.
    p = Packet::make(_headroom, (unsigned char *)0, _snaplen, 0);
    if (!p)
	return;
    p->take(p->length());

    if (f->pcap()) {
	pcap_dispatch(f->pcap(), 1, IPFlowRawSockets_get_packet, (u_char *) p);
	if (p->length() && fake_pcap_force_ip(p, f->datalink())) {
	    // Pull off the link header
	    p->pull((uintptr_t)p->ip_header() - (uintptr_t)p->data());
	    output(0).push(p);
	} else
	    p->kill();
    } else {
	// read data from socket
	len = read(fd, p->end_data(), p->tailroom());

	if (len > 0) {
	    p = p->put(len);
	    if (!p)
		return;

#ifdef SIOCGSTAMP
	    // set timestamp
	    p->timestamp_anno().set_timeval_ioctl(fd, SIOCGSTAMP);
#endif
	    // set IP annotations
	    if (fake_pcap_force_ip(p, FAKE_DLT_RAW)) {
		switch (p->ip_header()->ip_p) {
		case IPPROTO_TCP:
		    //click_chatter("TCP ports: %d %d", f->sport(),
		    //	      p->tcp_header()->th_dport);
		    if (f->sport() != p->tcp_header()->th_dport)
			goto drop;
		    break;
		case IPPROTO_UDP:
		    //click_chatter("UDP ports: %d %d", f->sport(),
		    //      p->udp_header()->uh_dport);
		    if (f->sport() != p->udp_header()->uh_dport)
			goto drop;
		    break;
		default:
		    break;
		}
		output(0).push(p);
		return;
	    }
	}

    drop:
	p->kill();
	if (len <= 0 && errno != EAGAIN)
	    errh->error("%s: read: %s", declaration().c_str(), strerror(errno));
    }
}

bool
IPFlowRawSockets::run_task(Task *)
{
    Packet *p = input(0).pull();
    if (p)
	push(0, p);
    else if (!_signal)
	return false;
    _task.fast_reschedule();
    return p != 0;
}

void
IPFlowRawSockets::aggregate_notify(uint32_t agg, AggregateEvent event, const Packet *)
{
    if (event == DELETE_AGG && find_aggregate(agg, 0)) {
	_gc_aggs.push_back(agg);
	_gc_aggs.push_back(click_jiffies());
	if (!_gc_timer.scheduled())
	    _gc_timer.schedule_after_msec(250);
    }
}

void
IPFlowRawSockets::gc_hook(Timer *t, void *thunk)
{
    IPFlowRawSockets *fs = static_cast<IPFlowRawSockets *>(thunk);
    uint32_t limit_jiff = click_jiffies() - (CLICK_HZ / 4);
    int i;
    for (i = 0; i < fs->_gc_aggs.size() && SEQ_LEQ(fs->_gc_aggs[i+1], limit_jiff); i += 2)
	if (Flow *f = fs->find_aggregate(fs->_gc_aggs[i], 0)) {
	    int bucket = (f->aggregate() & (NFLOWMAP - 1));
	    assert(fs->_flowmap[bucket] == f);
	    fs->_flowmap[bucket] = f->next();
	    fs->end_flow(f, ErrorHandler::default_handler());
	}
    if (i < fs->_gc_aggs.size()) {
	fs->_gc_aggs.erase(fs->_gc_aggs.begin(), fs->_gc_aggs.begin() + i);
	t->schedule_after_msec(250);
    }
}

enum { H_CLEAR };

int
IPFlowRawSockets::write_handler(const String &, Element *e, void *thunk, ErrorHandler *errh)
{
    IPFlowRawSockets *fs = static_cast<IPFlowRawSockets *>(e);
    switch ((intptr_t)thunk) {
      case H_CLEAR:
	for (int i = 0; i < NFLOWMAP; i++)
	    while (Flow *f = fs->_flowmap[i]) {
		fs->_flowmap[i] = f->next();
		fs->end_flow(f, errh);
	    }
	return 0;
      default:
	return -1;
    }
}

void
IPFlowRawSockets::add_handlers()
{
    add_write_handler("clear", write_handler, H_CLEAR);
    if (input_is_pull(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel AggregateNotifier pcap)
EXPORT_ELEMENT(IPFlowRawSockets)
