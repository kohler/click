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
 *
 * $Id: ipflowrawsockets.cc,v 1.1 2004/04/20 21:03:19 mhuang Exp $
 */

#include <click/config.h>
#include "ipflowrawsockets.hh"
#include <click/confparse.hh>
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
#include <arpa/inet.h>

#include <click/straccum.hh>

extern "C" {
#include <pcap.h>
}

#include "fakepcap.hh"

CLICK_DECLS

IPFlowRawSockets::Flow::Flow(const Packet *p)
    : _next(0),
      _flowid(p), _ip_p(p->ip_header()->ip_p),
      _aggregate(AGGREGATE_ANNO(p)),
      _wd(-1), _rd(-1), _pcap(0)
{
    if (PAINT_ANNO(p) & 1)	// reverse _flowid
	_flowid = _flowid.rev();
    
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
IPFlowRawSockets::Flow::initialize(ErrorHandler *errh, int snaplen)
{
    struct sockaddr_in sin;

    // open raw IP socket
    _wd = socket(PF_INET, SOCK_RAW, _ip_p);
    if (_wd < 0)
	return errh->error("socket: %s", strerror(errno));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = PF_INET;
    sin.sin_port = _flowid.sport();
    sin.sin_addr = inet_makeaddr(0, 0);

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
    int accept = 0;
    if (ioctl(pcap_fd, BIOCSSEESENT, &accept) != 0)
	return errh->error("ioctl: %s", strerror(errno));
#endif

    // build the BPF filter
    StringAccum ss;
    ss << "ip src host ";
    ss << _flowid.daddr().unparse().cc();
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
    : Element(1, 1), _nnoagg(0), _nagg(0), _agg_notifier(0), _task(this),
      _gc_timer(NULL)
{
    MOD_INC_USE_COUNT;
    for (int i = 0; i < NFLOWMAP; i++)
	_flowmap[i] = 0;
}

IPFlowRawSockets::~IPFlowRawSockets()
{
    MOD_DEC_USE_COUNT;
}

int
IPFlowRawSockets::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e = 0;

    _snaplen = 2046;
    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "NOTIFIER", cpElement, "aggregate deletion notifier", &e,
		    "SNAPLEN", cpUnsigned, "maximum packet length", &_snaplen,
		    cpEnd) < 0)
	return -1;

    if (e && !(_agg_notifier = (AggregateNotifier *)e->cast("AggregateNotifier")))
	return errh->error("%s is not an AggregateNotifier", e->id().cc());

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
    _gc_timer->cleanup();
    delete _gc_timer;
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
    _gc_timer = new Timer(gc_hook, this);
    assert(_gc_timer);
    _gc_timer->initialize(this);
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
	if (f->initialize(ErrorHandler::default_handler(), _snaplen)) {
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
    p->set_timestamp_anno(pkthdr->ts.tv_sec, pkthdr->ts.tv_usec);
    SET_EXTRA_LENGTH_ANNO(p, pkthdr->len - length);
}
}
CLICK_DECLS

void
IPFlowRawSockets::selected(int fd)
{
    ErrorHandler *errh = ErrorHandler::default_handler();

    // allocate packet
    WritablePacket *p = Packet::make(_snaplen);
    if (!p)
	return;
    p->take(p->length());

    assert(fd < _flows.size());
    Flow *f = _flows[fd];
    assert(f);
    if (f->pcap()) {
	// Read and push() at most one packet.
	pcap_dispatch(f->pcap(), 1, IPFlowRawSockets_get_packet, (u_char *) p);
	if (fake_pcap_force_ip(p, f->datalink())) {
	    // Pull off the link header
	    p->pull((unsigned)p->ip_header() - (unsigned)p->data());
	    output(0).push(p);
	} else
	    p->kill();
	return;
    }

    // set timestamp
    click_gettimeofday(&p->timestamp_anno());

    // read data from socket
    int r;
    do {
	while ((r = read(fd, p->end_data(), p->tailroom())) > 0) {
	    p = p->put(r);
	    assert(p);
	}
    } while (r < 0 && errno == EINTR);
    if (r < 0 && errno != EAGAIN) {
	errh->error("read: %s", strerror(errno));
	p->kill();
    } else
	output(0).push(p);
}

bool
IPFlowRawSockets::run_task()
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
	if (!_gc_timer->scheduled())
	    _gc_timer->schedule_after_ms(250);
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
	t->schedule_after_ms(250);
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
    add_write_handler("clear", write_handler, (void *)H_CLEAR);
    if (input_is_pull(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel analysis)
EXPORT_ELEMENT(IPFlowRawSockets)
