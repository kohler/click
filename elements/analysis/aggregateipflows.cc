/*
 * aggregateipflows.{cc,hh} -- set aggregate annotation based on TCP/UDP flow
 * Eddie Kohler
 *
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2005-2008 Regents of the University of California
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
#include "aggregateipflows.hh"
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include <click/packet_anno.hh>
#include <click/handlercall.hh>
CLICK_DECLS

#define SEC_OLDER(s1, s2)	((int)(s1 - s2) < 0)

// operations on host pairs and ports values

static inline const click_ip *
good_ip_header(const Packet *p)
{
    // called when we already know the packet is good
    const click_ip *iph = p->ip_header();
    if (iph->ip_p == IP_PROTO_ICMP)
	return reinterpret_cast<const click_ip *>(p->icmp_header() + 1); // know it exists
    else
	return iph;
}

static inline bool
operator==(const AggregateIPFlows::HostPair &a, const AggregateIPFlows::HostPair &b)
{
    return a.a == b.a && a.b == b.b;
}

inline hashcode_t
AggregateIPFlows::HostPair::hashcode() const
{
    return (a << 12) + b + ((a >> 20) & 0x1F);
}

static inline bool
ports_reverse_order(uint32_t ports)
{
    return (int32_t) ((ports << 16) - ports) < 0;
}

static inline uint32_t
flip_ports(uint32_t ports)
{
    return ((ports >> 16) & 0xFFFF) | (ports << 16);
}


// actual AggregateIPFlows operations

AggregateIPFlows::AggregateIPFlows()
#if CLICK_USERLEVEL
    : _traceinfo_file(0), _packet_source(0), _filepos_h(0)
#endif
{
}

AggregateIPFlows::~AggregateIPFlows()
{
}

void *
AggregateIPFlows::cast(const char *n)
{
    if (strcmp(n, "AggregateNotifier") == 0)
	return (AggregateNotifier *)this;
    else if (strcmp(n, "AggregateIPFlows") == 0)
	return (Element *)this;
    else
	return Element::cast(n);
}

int
AggregateIPFlows::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _tcp_timeout = 24 * 60 * 60;
    _tcp_done_timeout = 30;
    _udp_timeout = 60;
    _fragment_timeout = 30;
    _gc_interval = 20 * 60;
    _fragments = 2;
    bool handle_icmp_errors = false;
    bool fragments_parsed;
    bool fragments = true;

    if (Args(conf, this, errh)
	.read("TCP_TIMEOUT", _tcp_timeout)
	.read("TCP_DONE_TIMEOUT", _tcp_done_timeout)
	.read("UDP_TIMEOUT", SecondsArg(), _udp_timeout)
	.read("FRAGMENT_TIMEOUT", SecondsArg(), _fragment_timeout)
	.read("REAP", SecondsArg(), _gc_interval)
	.read("ICMP", handle_icmp_errors)
#if CLICK_USERLEVEL
	.read("TRACEINFO", FilenameArg(), _traceinfo_filename)
	.read("SOURCE", ElementArg(), _packet_source)
#endif
	.read("FRAGMENTS", fragments).read_status(fragments_parsed)
	.complete() < 0)
	return -1;

    _smallest_timeout = (_tcp_timeout < _tcp_done_timeout ? _tcp_timeout : _tcp_done_timeout);
    _smallest_timeout = (_smallest_timeout < _udp_timeout ? _smallest_timeout : _udp_timeout);
    _handle_icmp_errors = handle_icmp_errors;
    if (fragments_parsed)
	_fragments = fragments;
    return 0;
}

int
AggregateIPFlows::initialize(ErrorHandler *errh)
{
    _next = 1;
    _active_sec = _gc_sec = 0;
    _timestamp_warning = false;

#if CLICK_USERLEVEL
    if (_traceinfo_filename == "-")
	_traceinfo_file = stdout;
    else if (_traceinfo_filename && !(_traceinfo_file = fopen(_traceinfo_filename.c_str(), "w")))
	return errh->error("%s: %s", _traceinfo_filename.c_str(), strerror(errno));
    if (_traceinfo_file) {
	fprintf(_traceinfo_file, "<?xml version='1.0' standalone='yes'?>\n\
<trace");
	if (_packet_source) {
	    if (String s = HandlerCall::call_read(_packet_source, "filename").trim_space())
		fprintf(_traceinfo_file, " file='%s'", s.c_str());
	    (void) HandlerCall::reset_read(_filepos_h, _packet_source, "packet_filepos");
	}
	fprintf(_traceinfo_file, ">\n");
    }
#endif

    if (_fragments == 2)
	_fragments = !input_is_pull(0);
    else if (_fragments == 1 && input_is_pull(0))
	return errh->error("'FRAGMENTS true' is incompatible with pull; run this element in a push context");

    return 0;
}

void
AggregateIPFlows::cleanup(CleanupStage)
{
    clean_map(_tcp_map);
    clean_map(_udp_map);
#if CLICK_USERLEVEL
    if (_traceinfo_file && _traceinfo_file != stdout) {
	fprintf(_traceinfo_file, "</trace>\n");
	fclose(_traceinfo_file);
    }
    delete _filepos_h;
#endif
}

inline void
AggregateIPFlows::delete_flowinfo(const HostPair &hp, FlowInfo *finfo, bool really_delete)
{
#if CLICK_USERLEVEL
    if (_traceinfo_file) {
	StatFlowInfo *sinfo = static_cast<StatFlowInfo *>(finfo);
	IPAddress src(sinfo->reverse() ? hp.b : hp.a);
	int sport = (ntohl(sinfo->_ports) >> (sinfo->reverse() ? 0 : 16)) & 0xFFFF;
	IPAddress dst(sinfo->reverse() ? hp.a : hp.b);
	int dport = (ntohl(sinfo->_ports) >> (sinfo->reverse() ? 16 : 0)) & 0xFFFF;
	Timestamp duration = sinfo->_last_timestamp - sinfo->_first_timestamp;
	fprintf(_traceinfo_file, "<flow aggregate='%u' src='%s' sport='%d' dst='%s' dport='%d' begin='" PRITIMESTAMP "' duration='" PRITIMESTAMP "'",

		sinfo->_aggregate,
		src.unparse().c_str(), sport, dst.unparse().c_str(), dport,
		sinfo->_first_timestamp.sec(), sinfo->_first_timestamp.subsec(),
		duration.sec(), duration.subsec());
	if (sinfo->_filepos)
	    fprintf(_traceinfo_file, " filepos='%u'", sinfo->_filepos);
	fprintf(_traceinfo_file, ">\n\
  <stream dir='0' packets='%d' /><stream dir='1' packets='%d' />\n\
</flow>\n",
		sinfo->_packets[0], sinfo->_packets[1]);
	if (really_delete)
	    delete sinfo;
    } else
#endif
	if (really_delete)
	    delete finfo;
}

void
AggregateIPFlows::clean_map(Map &table)
{
    // free completed flows and emit fragments
    for (Map::iterator iter = table.begin(); iter.live(); iter++) {
	HostPairInfo *hpinfo = &iter.value();
	while (Packet *p = hpinfo->_fragment_head) {
	    hpinfo->_fragment_head = p->next();
	    p->kill();
	}
	while (FlowInfo *f = hpinfo->_flows) {
	    hpinfo->_flows = f->_next;
	    delete_flowinfo(iter.key(), f);
	}
    }
}

#if CLICK_USERLEVEL
void
AggregateIPFlows::stat_new_flow_hook(const Packet *p, FlowInfo *finfo)
{
    StatFlowInfo *sinfo = static_cast<StatFlowInfo *>(finfo);
    sinfo->_first_timestamp = p->timestamp_anno();
    sinfo->_filepos = 0;
    if (_filepos_h)
	(void) IntArg().parse(_filepos_h->call_read().trim_space(), sinfo->_filepos);
}
#endif

inline void
AggregateIPFlows::packet_emit_hook(const Packet *p, const click_ip *iph, FlowInfo *finfo)
{
    // account for timestamp
    finfo->_last_timestamp = p->timestamp_anno();

    // check whether this indicates the flow is over
    if (iph->ip_p == IP_PROTO_TCP && IP_FIRSTFRAG(iph)
	/* 3.Feb.2004 - NLANR dumps do not contain full TCP headers! So relax
	   the following length check to just make sure the flags are
	   there. */
	&& p->transport_length() >= 14
	&& PAINT_ANNO(p) < 2) {	// ignore ICMP errors
	if (p->tcp_header()->th_flags & TH_RST)
	    finfo->_flow_over = 3;
	else if (p->tcp_header()->th_flags & TH_FIN)
	    finfo->_flow_over |= (1 << PAINT_ANNO(p));
	else if (p->tcp_header()->th_flags & TH_SYN)
	    finfo->_flow_over = 0;
    }

#if CLICK_USERLEVEL
    // count packets
    if (stats() && PAINT_ANNO(p) < 2) {
	StatFlowInfo *sinfo = static_cast<StatFlowInfo *>(finfo);
	sinfo->_packets[PAINT_ANNO(p)]++;
    }
#endif
}

void
AggregateIPFlows::reap_map(Map &table, uint32_t timeout, uint32_t done_timeout)
{
    timeout = _active_sec - timeout;
    done_timeout = _active_sec - done_timeout;
    int frag_timeout = _active_sec - _fragment_timeout;

    // free completed flows and emit fragments
    for (Map::iterator iter = table.begin(); iter.live(); iter++) {
	HostPairInfo *hpinfo = &iter.value();
	Packet *head;
	// fragments
	while ((head = hpinfo->_fragment_head)
	       && (head->timestamp_anno().sec() < frag_timeout
		   || !IP_ISFRAG(good_ip_header(head))))
	    emit_fragment_head(hpinfo);

	// can't delete any flows if there are fragments
	if (hpinfo->_fragment_head)
	    continue;

	// completed flows
	FlowInfo **pprev = &hpinfo->_flows;
	FlowInfo *f = *pprev;
	while (f) {
	    // circular comparison
	    if (SEC_OLDER(f->_last_timestamp.sec(), (f->_flow_over == 3 ? done_timeout : timeout))) {
		notify(f->_aggregate, AggregateListener::DELETE_AGG, 0);
		*pprev = f->_next;
		delete_flowinfo(iter.key(), f);
	    } else
		pprev = &f->_next;
	    f = *pprev;
	}
	// XXX never free host pairs
    }
}

void
AggregateIPFlows::reap()
{
    if (_gc_sec) {
	reap_map(_tcp_map, _tcp_timeout, _tcp_done_timeout);
	reap_map(_udp_map, _udp_timeout, _udp_timeout);
    }
    _gc_sec = _active_sec + _gc_interval;
}

const click_ip *
AggregateIPFlows::icmp_encapsulated_header(const Packet *p)
{
    const click_icmp *icmph = p->icmp_header();
    if (p->has_transport_header()
	&& (icmph->icmp_type == ICMP_UNREACH
	    || icmph->icmp_type == ICMP_TIMXCEED
	    || icmph->icmp_type == ICMP_PARAMPROB
	    || icmph->icmp_type == ICMP_SOURCEQUENCH
	    || icmph->icmp_type == ICMP_REDIRECT)) {
	const click_ip *embedded_iph = reinterpret_cast<const click_ip *>(icmph + 1);
	unsigned embedded_hlen = embedded_iph->ip_hl << 2;
	if ((unsigned)p->transport_length() >= sizeof(click_icmp) + embedded_hlen
	    && embedded_hlen >= sizeof(click_ip))
	    return embedded_iph;
    }
    return 0;
}

int
AggregateIPFlows::relevant_timeout(const FlowInfo *f, const Map &m) const
{
    if (&m == &_udp_map)
	return _udp_timeout;
    else if (f->_flow_over == 3)
	return _tcp_done_timeout;
    else
	return _tcp_timeout;
}

// XXX timing when fragments are merged back in?

AggregateIPFlows::FlowInfo *
AggregateIPFlows::find_flow_info(Map &m, HostPairInfo *hpinfo, uint32_t ports, bool flipped, const Packet *p)
{
    FlowInfo **pprev = &hpinfo->_flows;
    for (FlowInfo *finfo = *pprev; finfo; pprev = &finfo->_next, finfo = finfo->_next)
	if (finfo->_ports == ports) {
	    // if this flow is actually dead (but has not yet been garbage
	    // collected), then kill it for consistent semantics
	    int age = p->timestamp_anno().sec() - finfo->_last_timestamp.sec();
	    // 4.Feb.2004 - Also start a new flow if the old flow closed off,
	    // and we have a SYN.
	    if ((age > (int) _smallest_timeout
		 && age > relevant_timeout(finfo, m))
		|| (finfo->_flow_over == 3
		    && p->ip_header()->ip_p == IP_PROTO_TCP
		    && (p->tcp_header()->th_flags & TH_SYN))) {
		// old aggregate has died
		notify(finfo->aggregate(), AggregateListener::DELETE_AGG, 0);
		const click_ip *iph = good_ip_header(p);
		HostPair hp(iph->ip_src.s_addr, iph->ip_dst.s_addr);
		delete_flowinfo(hp, finfo, false);

		// make a new aggregate
		finfo->_aggregate = _next;
		_next++;
		finfo->_reverse = flipped;
		finfo->_flow_over = 0;
#if CLICK_USERLEVEL
		if (stats())
		    stat_new_flow_hook(p, finfo);
#endif
		notify(finfo->aggregate(), AggregateListener::NEW_AGG, p);
	    }

	    // otherwise, move to the front of the list and return
	    *pprev = finfo->_next;
	    finfo->_next = hpinfo->_flows;
	    hpinfo->_flows = finfo;
	    return finfo;
	}

    // make and install new FlowInfo pair
    FlowInfo *finfo;
#if CLICK_USERLEVEL
    if (stats()) {
	finfo = new StatFlowInfo(ports, hpinfo->_flows, _next);
	stat_new_flow_hook(p, finfo);
    } else
#endif
	finfo = new FlowInfo(ports, hpinfo->_flows, _next);

    finfo->_reverse = flipped;
    hpinfo->_flows = finfo;
    _next++;
    notify(finfo->aggregate(), AggregateListener::NEW_AGG, p);
    return finfo;
}

void
AggregateIPFlows::emit_fragment_head(HostPairInfo *hpinfo)
{
    Packet *head = hpinfo->_fragment_head;
    hpinfo->_fragment_head = head->next();

    const click_ip *iph = good_ip_header(head);
    // XXX multiple linear traversals of entire fragment list!
    // want a faster method that takes up little memory?

    if (AGGREGATE_ANNO(head)) {
	for (Packet *p = hpinfo->_fragment_head; p; p = p->next())
	    if (good_ip_header(p)->ip_id == iph->ip_id) {
		SET_AGGREGATE_ANNO(p, AGGREGATE_ANNO(head));
		SET_PAINT_ANNO(p, PAINT_ANNO(head));
	    }
    } else {
	for (Packet *p = hpinfo->_fragment_head; p; p = p->next())
	    if (good_ip_header(p)->ip_id == iph->ip_id
		&& AGGREGATE_ANNO(p)) {
		SET_AGGREGATE_ANNO(head, AGGREGATE_ANNO(p));
		SET_PAINT_ANNO(head, PAINT_ANNO(p));
		goto find_flowinfo;
	    }
	head->kill();
	return;
    }

  find_flowinfo:
    // find the packet's FlowInfo
    FlowInfo *finfo, **pprev = &hpinfo->_flows;
    for (finfo = *pprev; finfo; pprev = &finfo->_next, finfo = *pprev)
	if (finfo->_aggregate == AGGREGATE_ANNO(head)) {
	    *pprev = finfo->_next;
	    finfo->_next = hpinfo->_flows;
	    hpinfo->_flows = finfo;
	    break;
	}

    assert(finfo);
    packet_emit_hook(head, iph, finfo);
    output(0).push(head);
}

int
AggregateIPFlows::handle_fragment(Packet *p, HostPairInfo *hpinfo)
{
    if (hpinfo->_fragment_head)
	hpinfo->_fragment_tail->set_next(p);
    else
	hpinfo->_fragment_head = p;
    hpinfo->_fragment_tail = p;
    p->set_next(0);
    _active_sec = p->timestamp_anno().sec();

    // get rid of old fragments
    int frag_timeout = _active_sec - _fragment_timeout;
    Packet *head;
    while ((head = hpinfo->_fragment_head)
	   && (head->timestamp_anno().sec() < frag_timeout
	       || !IP_ISFRAG(good_ip_header(head))))
	emit_fragment_head(hpinfo);

    return ACT_NONE;
}

int
AggregateIPFlows::handle_packet(Packet *p)
{
    const click_ip *iph = p->ip_header();
    int paint = 0;

    // assign timestamp if no timestamp given
    if (!p->timestamp_anno()) {
	if (!_timestamp_warning) {
	    click_chatter("%{element}: warning: packet received without timestamp", this);
	    _timestamp_warning = true;
	}
	p->timestamp_anno().assign_now();
    }

    // extract encapsulated ICMP header if appropriate
    if (p->has_network_header() && iph->ip_p == IP_PROTO_ICMP
	&& IP_FIRSTFRAG(iph) && _handle_icmp_errors) {
	iph = icmp_encapsulated_header(p);
	paint = 2;
    }

    // return if not a proper TCP/UDP packet
    if (!p->has_network_header()
	|| (iph->ip_p != IP_PROTO_TCP && iph->ip_p != IP_PROTO_UDP)
	|| (iph->ip_src.s_addr == 0 && iph->ip_dst.s_addr == 0))
	return ACT_DROP;

    // find relevant HostPairInfo
    Map &m = (iph->ip_p == IP_PROTO_TCP ? _tcp_map : _udp_map);
    HostPair hosts(iph->ip_src.s_addr, iph->ip_dst.s_addr);
    if (hosts.a != iph->ip_src.s_addr)
	paint ^= 1;
    HostPairInfo *hpinfo = &m[hosts];

    // find relevant FlowInfo, if any
    FlowInfo *finfo;
    if (IP_FIRSTFRAG(iph)) {
	const uint8_t *udp_ptr = reinterpret_cast<const uint8_t *>(iph) + (iph->ip_hl << 2);
	if (udp_ptr + 4 > p->end_data())
	    // packet not big enough
	    return ACT_DROP;

	uint32_t ports = *reinterpret_cast<const uint32_t *>(udp_ptr);
	// 1.Jan.08: handle connections where IP addresses are the same (John
	// Russell Lane)
	if (hosts.a == hosts.b && ports_reverse_order(ports))
	    paint ^= 1;
	if (paint & 1)
	    ports = flip_ports(ports);

	finfo = find_flow_info(m, hpinfo, ports, paint & 1, p);
	if (!finfo) {
	    click_chatter("out of memory!");
	    return ACT_DROP;
	}
	if (finfo->reverse())
	    paint ^= 1;

	// set aggregate annotations
	SET_AGGREGATE_ANNO(p, finfo->aggregate());
	SET_PAINT_ANNO(p, paint);
    } else {
	finfo = 0;
	SET_AGGREGATE_ANNO(p, 0);
	SET_PAINT_ANNO(p, paint);
    }

    // check for fragment
    if ((_fragments && IP_ISFRAG(iph)) || hpinfo->_fragment_head)
	return handle_fragment(p, hpinfo);
    else if (!finfo)
	return ACT_DROP;

    // packet emit hook
    _active_sec = p->timestamp_anno().sec();
    packet_emit_hook(p, iph, finfo);

    return ACT_EMIT;
}

void
AggregateIPFlows::push(int, Packet *p)
{
    int action = handle_packet(p);

    // GC if necessary
    if (_active_sec >= _gc_sec)
	reap();

    if (action == ACT_EMIT)
	output(0).push(p);
    else if (action == ACT_DROP)
	checked_output_push(1, p);
}

Packet *
AggregateIPFlows::pull(int)
{
    Packet *p = input(0).pull();
    int action = (p ? handle_packet(p) : ACT_NONE);

    // GC if necessary
    if (_active_sec >= _gc_sec)
	reap();

    if (action == ACT_EMIT)
	return p;
    else if (action == ACT_DROP)
	checked_output_push(1, p);
    return 0;
}

enum { H_CLEAR };

int
AggregateIPFlows::write_handler(const String &, Element *e, void *thunk, ErrorHandler *)
{
    AggregateIPFlows *af = static_cast<AggregateIPFlows *>(e);
    switch ((intptr_t)thunk) {
      case H_CLEAR: {
	  int active_sec = af->_active_sec, gc_sec = af->_gc_sec;
	  af->_active_sec = af->_gc_sec = 0x7FFFFFFF;
	  af->reap();
	  af->_active_sec = active_sec, af->_gc_sec = gc_sec;
	  return 0;
      }
      default:
	return -1;
    }
}

void
AggregateIPFlows::add_handlers()
{
    add_write_handler("clear", write_handler, H_CLEAR);
}

ELEMENT_REQUIRES(AggregateNotifier)
EXPORT_ELEMENT(AggregateIPFlows)
CLICK_ENDDECLS
