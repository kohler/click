// -*- mode: c++; c-basic-offset: 4 -*-
#include <click/config.h>
#include "aggregateipflows.hh"
#include <click/error.hh>
#include <click/hashmap.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
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

static inline uint32_t
hashcode(const AggregateIPFlows::HostPair &a)
{
    return a.a ^ a.b;
}

static inline uint32_t
flip_ports(uint32_t ports)
{
    return ((ports >> 16) & 0xFFFF) | (ports << 16);
}


// actual AggregateIPFlows operations

AggregateIPFlows::AggregateIPFlows()
    : Element(1, 1), _traceinfo_file(0), _packet_source(0), _filepos_h(0)
{
    MOD_INC_USE_COUNT;
}

AggregateIPFlows::~AggregateIPFlows()
{
    MOD_DEC_USE_COUNT;
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

void
AggregateIPFlows::notify_noutputs(int n)
{
    set_noutputs(n <= 1 ? 1 : 2);
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
    bool gave_fragments = false, fragments = true;
    
    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "TCP_TIMEOUT", cpSeconds, "timeout for active TCP connections", &_tcp_timeout,
		    "TCP_DONE_TIMEOUT", cpSeconds, "timeout for completed TCP connections", &_tcp_done_timeout,
		    "UDP_TIMEOUT", cpSeconds, "timeout for UDP connections", &_udp_timeout,
		    "FRAGMENT_TIMEOUT", cpSeconds, "timeout for fragment collection", &_fragment_timeout,
		    "REAP", cpSeconds, "garbage collection interval", &_gc_interval,
		    "ICMP", cpBool, "handle ICMP errors?", &handle_icmp_errors,
		    "TRACEINFO", cpFilename, "filename for connection info file", &_traceinfo_filename,
		    "SOURCE", cpElement, "packet source element", &_packet_source,
		    cpConfirmKeywords,
		    "FRAGMENTS", cpBool, "handle fragmented packets?", &gave_fragments, &fragments,
		    0) < 0)
	return -1;
    
    _smallest_timeout = (_tcp_timeout < _tcp_done_timeout ? _tcp_timeout : _tcp_done_timeout);
    _smallest_timeout = (_smallest_timeout < _udp_timeout ? _smallest_timeout : _udp_timeout);
    _handle_icmp_errors = handle_icmp_errors;
    if (gave_fragments)
	_fragments = fragments;
    return 0;
}

int
AggregateIPFlows::initialize(ErrorHandler *errh)
{
    _next = 1;
    _active_sec = _gc_sec = 0;
    _timestamp_warning = false;
    
    if (_traceinfo_filename == "-")
	_traceinfo_file = stdout;
    else if (_traceinfo_filename && !(_traceinfo_file = fopen(_traceinfo_filename.cc(), "w")))
	return errh->error("%s: %s", _traceinfo_filename.cc(), strerror(errno));
    if (_traceinfo_file) {
	fprintf(_traceinfo_file, "<?xml version='1.0' standalone='yes'?>\n\
<trace");
	if (_packet_source) {
	    if (String s = HandlerCall::call_read(_packet_source, "filename").trim_space())
		fprintf(_traceinfo_file, " file='%s'", s.cc());
	    (void) HandlerCall::reset_read(_filepos_h, _packet_source, "packet_filepos");
	}
	fprintf(_traceinfo_file, ">\n");
    }

    if (_fragments == 2)
	_fragments = !input_is_pull(0);
    else if (_fragments == 1 && input_is_pull(0))
	return errh->error("`FRAGMENTS true' is incompatible with pull; run this element in a push context");
    
    return 0;
}

void
AggregateIPFlows::cleanup(CleanupStage)
{
    clean_map(_tcp_map);
    clean_map(_udp_map);
    if (_traceinfo_file && _traceinfo_file != stdout) {
	fprintf(_traceinfo_file, "</trace>\n");
	fclose(_traceinfo_file);
    }
    delete _filepos_h;
}

inline void
AggregateIPFlows::delete_flowinfo(const HostPair &hp, FlowInfo *finfo, bool really_delete)
{
    if (_traceinfo_file) {
	StatFlowInfo *sinfo = static_cast<StatFlowInfo *>(finfo);
	IPAddress src(sinfo->reverse() ? hp.b : hp.a);
	int sport = (ntohl(sinfo->_ports) >> (sinfo->reverse() ? 0 : 16)) & 0xFFFF;
	IPAddress dst(sinfo->reverse() ? hp.a : hp.b);
	int dport = (ntohl(sinfo->_ports) >> (sinfo->reverse() ? 16 : 0)) & 0xFFFF;
	struct timeval duration = sinfo->_last_timestamp - sinfo->_first_timestamp;
	fprintf(_traceinfo_file, "<flow aggregate='%u' src='%s' sport='%d' dst='%s' dport='%d' begin='%ld.%06ld' duration='%ld.%06ld'",

		sinfo->_aggregate,
		src.s().cc(), sport, dst.s().cc(), dport,
		sinfo->_first_timestamp.tv_sec, sinfo->_first_timestamp.tv_usec,
		duration.tv_sec, duration.tv_usec);
	if (sinfo->_filepos)
	    fprintf(_traceinfo_file, " filepos='%u'", sinfo->_filepos);
	fprintf(_traceinfo_file, ">\n\
  <stream dir='0' packets='%d' /><stream dir='1' packets='%d' />\n\
</flow>\n",
		sinfo->_packets[0], sinfo->_packets[1]);
	if (really_delete)
	    delete sinfo;
    } else
	if (really_delete)
	    delete finfo;
}

void
AggregateIPFlows::clean_map(Map &table)
{
    // free completed flows and emit fragments
    for (Map::iterator iter = table.begin(); iter; iter++) {
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

void
AggregateIPFlows::stat_new_flow_hook(const Packet *p, FlowInfo *finfo)
{
    StatFlowInfo *sinfo = static_cast<StatFlowInfo *>(finfo);
    sinfo->_first_timestamp = p->timestamp_anno();
    sinfo->_filepos = 0;
    if (_filepos_h)
	(void) cp_unsigned(_filepos_h->call_read().trim_space(), &sinfo->_filepos);
}

inline void
AggregateIPFlows::packet_emit_hook(const Packet *p, const click_ip *iph, FlowInfo *finfo)
{
    // check whether this indicates the flow is over
    if (iph->ip_p == IP_PROTO_TCP && IP_FIRSTFRAG(iph)
	&& p->transport_length() >= (int)sizeof(click_tcp)
	&& PAINT_ANNO(p) < 2) {	// ignore ICMP errors
	if (p->tcp_header()->th_flags & TH_RST)
	    finfo->_flow_over = 3;
	else if (p->tcp_header()->th_flags & TH_FIN)
	    finfo->_flow_over |= (1 << PAINT_ANNO(p));
	else if (p->tcp_header()->th_flags & TH_SYN)
	    finfo->_flow_over = 0;
    }

    // count packets
    if (stats() && PAINT_ANNO(p) < 2) {
	StatFlowInfo *sinfo = static_cast<StatFlowInfo *>(finfo);
	sinfo->_packets[PAINT_ANNO(p)]++;
    }
}

void
AggregateIPFlows::assign_aggregate(Map &table, HostPairInfo *hpinfo, int emit_before_sec)
{
    Packet *first = hpinfo->_fragment_head;
    uint16_t want_ip_id = good_ip_header(first)->ip_id;

    // find FlowInfo
    FlowInfo *finfo = 0;
    if (AGGREGATE_ANNO(first)) {
	for (finfo = hpinfo->_flows; finfo && finfo->_aggregate != AGGREGATE_ANNO(first); finfo = finfo->_next)
	    /* nada */;
    } else {
	for (Packet *p = first; !finfo && p; p = p->next()) {
	    const click_ip *iph = good_ip_header(p);
	    if (iph->ip_id == want_ip_id) {
		if (IP_FIRSTFRAG(iph)) {
		    uint32_t ports = *(reinterpret_cast<const uint32_t *>(reinterpret_cast<const uint8_t *>(iph) + (iph->ip_hl << 2)));
		    if (PAINT_ANNO(p) & 1)
			ports = flip_ports(ports);
		    finfo = find_flow_info(table, hpinfo, ports, PAINT_ANNO(p) & 1, p);
		}
	    }
	}
    }

    // if no FlowInfo found, delete packet
    if (!finfo) {
	hpinfo->_fragment_head = first->next();
	first->kill();
	return;
    }

    // emit packets at the beginning of the list that have the same IP ID
    const click_ip *iph;
    while (first && (iph = good_ip_header(first)) && iph->ip_id == want_ip_id
	   && (SEC_OLDER(first->timestamp_anno().tv_sec, emit_before_sec)
	       || !IP_ISFRAG(iph))) {
	Packet *p = first;
	hpinfo->_fragment_head = first = p->next();
	p->set_next(0);
	bool was_fragment = IP_ISFRAG(iph);

	SET_AGGREGATE_ANNO(p, finfo->aggregate());

	// actually emit packet
	if (finfo->reverse())
	    SET_PAINT_ANNO(p, PAINT_ANNO(p) ^ 1);
	finfo->_last_timestamp = p->timestamp_anno();

	packet_emit_hook(p, iph, finfo);
	
	output(0).push(p);

	// if not a fragment, know we don't have more fragments
	if (!was_fragment)
	    return;
    }
    
    // assign aggregate annotation to other packets with the same IP ID
    for (Packet *p = first; p; p = p->next())
	if (good_ip_header(p)->ip_id == want_ip_id)
	    SET_AGGREGATE_ANNO(p, finfo->aggregate());
}

void
AggregateIPFlows::reap_map(Map &table, uint32_t timeout, uint32_t done_timeout)
{
    timeout = _active_sec - timeout;
    done_timeout = _active_sec - done_timeout;
    int frag_timeout = _active_sec - _fragment_timeout;

    // free completed flows and emit fragments
    for (Map::iterator iter = table.begin(); iter; iter++) {
	HostPairInfo *hpinfo = &iter.value();
	// fragments
	while (hpinfo->_fragment_head && hpinfo->_fragment_head->timestamp_anno().tv_sec < frag_timeout)
	    assign_aggregate(table, hpinfo, frag_timeout);

	// can't delete any flows if there are fragments
	if (hpinfo->_fragment_head)
	    continue;

	// completed flows
	FlowInfo **pprev = &hpinfo->_flows;
	FlowInfo *f = *pprev;
	while (f) {
	    // circular comparison
	    if (SEC_OLDER(f->_last_timestamp.tv_sec, (f->_flow_over == 3 ? done_timeout : timeout))) {
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
    if (icmph
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
	    int age = p->timestamp_anno().tv_sec - finfo->_last_timestamp.tv_sec;
	    if (age > _smallest_timeout
		&& age > relevant_timeout(finfo, m)) {
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
		if (stats())
		    stat_new_flow_hook(p, finfo);
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
    if (stats()) {
	finfo = new StatFlowInfo(ports, hpinfo->_flows, _next);
	stat_new_flow_hook(p, finfo);
    } else
	finfo = new FlowInfo(ports, hpinfo->_flows, _next);
    
    finfo->_reverse = flipped;
    hpinfo->_flows = finfo;
    _next++;
    notify(finfo->aggregate(), AggregateListener::NEW_AGG, p);
    return finfo;
}

int
AggregateIPFlows::handle_fragment(Packet *p, int paint, Map &table, HostPairInfo *hpinfo)
{
    if (hpinfo->_fragment_tail)
	hpinfo->_fragment_tail->set_next(p);
    else
	hpinfo->_fragment_head = p;
    hpinfo->_fragment_tail = p;
    p->set_next(0);
    SET_AGGREGATE_ANNO(p, 0);
    SET_PAINT_ANNO(p, paint);
    if (int p_sec = p->timestamp_anno().tv_sec)
	_active_sec = p_sec;

    // get rid of old fragments
    int frag_timeout = _active_sec - _fragment_timeout;
    Packet *head;
    while ((head = hpinfo->_fragment_head)
	   && (head->timestamp_anno().tv_sec < frag_timeout || !IP_ISFRAG(good_ip_header(head))))
	assign_aggregate(table, hpinfo, frag_timeout);

    return ACT_NONE;
}

int
AggregateIPFlows::handle_packet(Packet *p)
{
    const click_ip *iph = p->ip_header();
    int paint = 0;

    // assign timestamp if no timestamp given
    if (!p->timestamp_anno().tv_sec) {
	if (!_timestamp_warning) {
	    click_chatter("%{element}: warning: packet received without timestamp", this);
	    _timestamp_warning = true;
	}
	click_gettimeofday(&p->timestamp_anno());
    }
	
    // extract encapsulated ICMP header if appropriate
    if (iph && iph->ip_p == IP_PROTO_ICMP && IP_FIRSTFRAG(iph)
	&& _handle_icmp_errors) {
	iph = icmp_encapsulated_header(p);
	paint = 2;
    }

    // return if not a proper TCP/UDP packet
    if (!iph
	|| (iph->ip_p != IP_PROTO_TCP && iph->ip_p != IP_PROTO_UDP)
	|| (iph->ip_src.s_addr == 0 && iph->ip_dst.s_addr == 0))
	return ACT_DROP;
    
    const uint8_t *udp_ptr = reinterpret_cast<const uint8_t *>(iph) + (iph->ip_hl << 2);
    if ((udp_ptr + sizeof(click_udp)) - p->data() > (int) p->length())
	// packet not big enough
	return ACT_DROP;

    // find relevant FlowInfo
    Map &m = (iph->ip_p == IP_PROTO_TCP ? _tcp_map : _udp_map);
    HostPair hosts(iph->ip_src.s_addr, iph->ip_dst.s_addr);
    if (hosts.a != iph->ip_src.s_addr)
	paint ^= 1;
    HostPairInfo *hpinfo = m.findp_force(hosts);

    // check for fragment
    if (IP_ISFRAG(iph)) {
	if (IP_FIRSTFRAG(iph) || _fragments)
	    return handle_fragment(p, paint, m, hpinfo);
	else
	    return ACT_DROP;
    } else if (hpinfo->_fragment_head)
	return handle_fragment(p, paint, m, hpinfo);
    
    uint32_t ports = *(reinterpret_cast<const uint32_t *>(udp_ptr));
    if (paint & 1)
	ports = flip_ports(ports);
    FlowInfo *finfo = find_flow_info(m, hpinfo, ports, paint & 1, p);
    
    if (!finfo) {
	click_chatter("out of memory!");
	return ACT_DROP;
    }

    // mark packet with aggregate number and paint
    finfo->_last_timestamp.tv_sec = _active_sec = p->timestamp_anno().tv_sec;
    finfo->_last_timestamp.tv_usec = p->timestamp_anno().tv_usec;
    SET_AGGREGATE_ANNO(p, finfo->aggregate());
    if (finfo->reverse())
	paint ^= 1;
    SET_PAINT_ANNO(p, paint);

    // packet emit hook
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
    add_write_handler("clear", write_handler, (void *)H_CLEAR);
}

ELEMENT_REQUIRES(userlevel AggregateNotifier)
EXPORT_ELEMENT(AggregateIPFlows)
#include <click/bighashmap.cc>
CLICK_ENDDECLS
