// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * iprw.{cc,hh} -- rewrites packet source and destination
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "iprw.hh"
#include "elements/ip/iprwpatterns.hh"
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>

#ifdef CLICK_LINUXMODULE
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
# include <asm/softirq.h>
#endif
#include <net/sock.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
#endif

CLICK_DECLS


//
// IPRw::Mapping
//

IPRw::Mapping::Mapping(bool dst_anno)
  : _flags(dst_anno ? F_DST_ANNO : 0), _ip_p(0), _pat(0), _free_next(0)
{
}

void
IPRw::Mapping::initialize(int ip_p, const IPFlowID &in, const IPFlowID &out,
			  int output, uint16_t flags, Mapping *reverse)
{
    // set fields
    _ip_p = ip_p;
    _mapto = out;
    _output = output;
    assert(output >= 0 && output < 256);
    _flags |= flags;
    _reverse = reverse;

    // set checksum deltas
    const unsigned short* source_words = (const unsigned short*)&in;
    const unsigned short* dest_words = (const unsigned short*)&_mapto;
    unsigned delta = 0;
    for (int i = 0; i < 4; i++) {
	delta += ~source_words[i] & 0xFFFF;
	delta += dest_words[i];
    }
    delta = (delta & 0xFFFF) + (delta >> 16);
    _ip_csum_delta = delta + (delta >> 16);

    for (int i = 4; i < 6; i++) {
	delta += ~source_words[i] & 0xFFFF;
	delta += dest_words[i];
    }
    delta = (delta & 0xFFFF) + (delta >> 16);
    _udp_csum_delta = delta + (delta >> 16);
}

void
IPRw::Mapping::make_pair(int ip_p, const IPFlowID &inf, const IPFlowID &outf,
			 int foutput, int routput,
			 Mapping *in_map, Mapping *out_map)
{
    in_map->initialize(ip_p, inf, outf, foutput, 0, out_map);
    out_map->initialize(ip_p, outf.reverse(), inf.reverse(), routput, F_REVERSE, in_map);
}

void
IPRw::Mapping::apply(WritablePacket *p)
{
    assert(p->has_network_header());
    click_ip *iph = p->ip_header();

    // IP header
    iph->ip_src = _mapto.saddr();
    iph->ip_dst = _mapto.daddr();
    if (_flags & F_DST_ANNO)
	p->set_dst_ip_anno(_mapto.daddr());

    unsigned sum = (~iph->ip_sum & 0xFFFF) + _ip_csum_delta;
    sum = (sum & 0xFFFF) + (sum >> 16);
    iph->ip_sum = ~(sum + (sum >> 16));

    mark_used();

    // end if not first fragment
    if (!IP_FIRSTFRAG(iph))
	return;

    // UDP/TCP header
    if (_ip_p == IP_PROTO_TCP) {

	click_tcp *tcph = p->tcp_header();
	tcph->th_sport = _mapto.sport();
	tcph->th_dport = _mapto.dport();
	click_update_in_cksum(&tcph->th_sum, 0xFFFF, _udp_csum_delta);

	// check for session ending flags
	if (tcph->th_flags & TH_RST)
	    set_session_over();
	else if (tcph->th_flags & TH_FIN)
	    set_session_flow_over();
	else if (tcph->th_flags & TH_SYN)
	    clear_session_flow_over();

    } else if (_ip_p == IP_PROTO_UDP) {

	click_udp *udph = p->udp_header();
	udph->uh_sport = _mapto.sport();
	udph->uh_dport = _mapto.dport();
	if (udph->uh_sum)	// 0 checksum is no checksum
	    click_update_in_cksum(&udph->uh_sum, 0xFFFF, _udp_csum_delta);

    }
}

String
IPRw::Mapping::unparse() const
{
    StringAccum sa;
    sa << reverse()->flow_id().reverse() << " => " << flow_id() << " [" << output() << "]";
    return sa.take_string();
}

//
// IPRw::Pattern
//

IPRw::Pattern::Pattern(const IPAddress &saddr, int sport,
		       const IPAddress &daddr, int dport,
		       bool is_napt, bool sequential, uint32_t variation_top)
    : _saddr(saddr), _sport(sport), _daddr(daddr), _dport(dport),
      _variation_top(variation_top), _next_variation(0), _is_napt(is_napt),
      _sequential(sequential), _refcount(0), _nmappings(0)
{
    if (_variation_top > 0)
	for (_variation_mask = 1; _variation_mask < _variation_top; )
	    _variation_mask = (_variation_mask << 1) | 1;
}

namespace {
enum { PE_NAPT, PE_NAT, PE_SADDR, PE_SPORT, PE_DADDR, PE_DPORT };
static const char* const pe_messages[] = {
    "syntax error, expected 'SADDR SPORT[-SPORTH] DADDR DPORT'",
    "syntax error, expected 'SADDR[-SADDRH] DADDR'",
    "bad source address",
    "bad source port",
    "bad destination address",
    "bad destination port"
};
static inline int
pattern_error(int what, ErrorHandler* errh)
{
    return errh->error(pe_messages[what]);
}
}

int
IPRw::Pattern::parse_napt(Vector<String> &words, Pattern **pstore,
			  Element *e, ErrorHandler *errh)
{
    if (words.size() != 4)
	return pattern_error(PE_NAPT, errh);

    IPAddress saddr, daddr;
    int32_t sportl, sporth, dport;
    bool sequential = false;

    if (words[0] == "-")
	saddr = 0;
    else if (!cp_ip_address(words[0], &saddr, e))
	return pattern_error(PE_SADDR, errh);

    if (words[1] == "-")
	sportl = sporth = 0;
    else {
	const char* end = words[1].end();
	if (end > words[1].begin() && end[-1] == '#')
	    sequential = true, end--;
	const char* dash = find(words[1].begin(), end, '-');
	if (!(cp_integer(words[1].substring(words[1].begin(), dash), &sportl)
	      && sportl > 0 && sportl <= 0xFFFF
	      && (dash == end ? (sporth = sportl)
		  : (cp_integer(words[1].substring(dash + 1, end), &sporth)
		     && sporth >= sportl && sporth <= 0xFFFF))))
	    return pattern_error(PE_SPORT, errh);
    }

    if (words[2] == "-")
	daddr = 0;
    else if (!cp_ip_address(words[2], &daddr, e))
	return pattern_error(PE_DADDR, errh);

    if (words[3] == "-")
	dport = 0;
    else if (!cp_integer(words[3], &dport) || dport <= 0 || dport > 0xFFFF)
	return pattern_error(PE_DPORT, errh);

    *pstore = new Pattern(saddr, htons(sportl), daddr, htons(dport), true, sequential, sporth - sportl);
    return 0;
}

int
IPRw::Pattern::parse_nat(Vector<String> &words, Pattern **pstore,
			 Element *e, ErrorHandler *errh)
{
    if (words.size() != 1 && words.size() != 2)
	return pattern_error(PE_NAT, errh);

    IPAddress saddr1, saddr2;
    bool sequential = false;
    if (words[0] == "-")
	saddr1 = saddr2 = 0;
    else {
	const char* end = words[0].end();
	if (end > words[0].begin() && end[-1] == '#')
	    sequential = true, end--;
	const char* dash = find(words[0].begin(), end, '-');
	if (dash == end) {
	    if (!cp_ip_prefix(words[0].substring(words[0].begin(), end), &saddr1, &saddr2, true, e) || !saddr1 || !saddr2)
		return pattern_error(PE_SADDR, errh);
	    if (saddr2 == IPAddress(0xFFFFFFFFU))
		saddr2 = saddr1;
	    else if (saddr2 == IPAddress(htonl(0xFFFFFFFEU))) {
		saddr1 &= saddr2;
		saddr2 = saddr1 | IPAddress(htonl(0x00000001U));
	    } else {
		uint32_t s1 = saddr1 & saddr2;
		uint32_t s2 = saddr1 | ~saddr2;
		// don't count PREFIX.0 and PREFIX.255
		saddr1 = htonl(ntohl(s1) + 1);
		saddr2 = htonl(ntohl(s2) - 1);
	    }
	} else if (!cp_ip_address(words[0].substring(words[0].begin(), dash), &saddr1, e)
		   || !cp_ip_address(words[0].substring(dash+1, end), &saddr2, e)
		   || ntohl(saddr1.addr()) > ntohl(saddr2.addr()))
	    return pattern_error(PE_SADDR, errh);
    }

    IPAddress daddr;
    if (words.size() == 1 || words[1] == "-")
	daddr = 0;
    else if (!cp_ip_address(words[1], &daddr, e) || !daddr)
	return pattern_error(PE_DADDR, errh);

    *pstore = new Pattern(saddr1, 0, daddr, 0, false, sequential, ntohl(saddr2.addr()) - ntohl(saddr1.addr()));
    return 0;
}

int
IPRw::Pattern::parse(const String &conf, Pattern **pstore,
		     Element *e, ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(conf, words);

    // check for IPRewriterPatterns reference
    if (words.size() == 1) {
	if (Pattern *p = IPRewriterPatterns::find(e, words[0], errh)) {
	    *pstore = p;
	    return 0;
	} else
	    return -1;
    } else if (words.size() == 2)
	return parse_nat(words, pstore, e, errh);
    else
	return parse_napt(words, pstore, e, errh);
}

int
IPRw::Pattern::parse_with_ports(const String &conf, Pattern **pstore,
				int *fport_store, int *rport_store,
				Element *e, ErrorHandler *errh)
{
    Vector<String> words;
    cp_spacevec(conf, words);
    int32_t fport, rport;

    if (words.size() <= 2
	|| !cp_integer(words[words.size() - 2], &fport)
	|| !cp_integer(words[words.size() - 1], &rport)
	|| fport < 0 || rport < 0)
	return errh->error("bad forward and/or reverse ports in pattern spec");
    words.resize(words.size() - 2);

    // check for IPRewriterPatterns reference
    Pattern *p;
    if (parse(cp_unspacevec(words), &p, e, errh) >= 0) {
	*pstore = p;
	*fport_store = fport;
	*rport_store = rport;
	return 0;
    } else
	return -1;
}

bool
IPRw::Pattern::can_accept_from(const Pattern &o) const
{
    if (_daddr != o._daddr || _dport != o._dport || _is_napt != o._is_napt
	|| (_is_napt ? _saddr != o._saddr : _sport != o._sport))
	return false;
    uint32_t low, o_low;
    if (_is_napt)
	low = ntohs(_sport), o_low = ntohs(o._sport);
    else
	low = ntohl(_saddr.addr()), o_low = ntohl(o._saddr.addr());
    uint32_t high = low + _variation_top;
    uint32_t o_high = o_low + _variation_top;
    return low <= o_low && o_high <= high;
}

bool
IPRw::Pattern::create_mapping(int ip_p, const IPFlowID& in,
			      int fport, int rport,
			      Mapping* fmap, Mapping* rmap,
			      const Map& rev_map)
{
    IPFlowID out(in);
    if (_saddr)
	out.set_saddr(_saddr);
    if (_sport)
	out.set_sport(_sport);
    if (_daddr)
	out.set_daddr(_daddr);
    if (_dport)
	out.set_dport(_dport);

    if (_variation_top) {
	uint32_t val = (_sequential ? _next_variation : click_random() & _variation_mask);
	uint32_t step = (_sequential ? 1 : click_random() | 1);
	uint32_t base = (_is_napt ? ntohs(_sport) : ntohl(_saddr.addr()));
	IPFlowID lookup = out.reverse();
	for (uint32_t count = 0; count <= _variation_mask; count++, val = (val + step) & _variation_mask)
	    if (val <= _variation_top) {
		if (_is_napt)
		    lookup.set_dport(htons(base + val));
		else
		    lookup.set_daddr(htonl(base + val));
		if (!rev_map.find(lookup)) {
		    if (_is_napt)
			out.set_sport(lookup.dport());
		    else
			out.set_saddr(lookup.daddr());
		    _next_variation = val + 1;
		    goto found;
		}
	    }
	return false;
    }

  found:
    Mapping::make_pair(ip_p, in, out, fport, rport, fmap, rmap);
    accept_mapping(fmap);
    return true;
}

void
IPRw::Pattern::accept_mapping(Mapping *m)
{
    m->_pat = this;
    _nmappings++;
}

inline void
IPRw::Pattern::mapping_freed(Mapping *)
{
    _nmappings--;
}

String
IPRw::Pattern::unparse() const
{
    StringAccum sa;
    if (!_is_napt && _variation_top)
	sa << _saddr << '-'
	   << IPAddress(htonl(ntohl(_saddr.addr()) + _variation_top));
    else if (_saddr)
	sa << _saddr;
    else
	sa << '-';

    if (!_is_napt)
	/* nada */;
    else if (!_sport)
	sa << " -";
    else if (_variation_top)
	sa << ' ' << ntohs(_sport) << '-' << (ntohs(_sport) + _variation_top);
    else
	sa << ' ' << ntohs(_sport);

    if (_daddr)
	sa << ' ' << _daddr;
    else
	sa << " -";

    if (!_is_napt)
	/* nada */;
    else if (!_dport)
	sa << " -";
    else
	sa << ' ' << ntohs(_dport);

    sa << " [" << _nmappings << ']';

    return sa.take_string();
}

//
// IPMapper
//

void
IPMapper::notify_rewriter(IPRw *, ErrorHandler *)
{
}

IPRw::Mapping *
IPMapper::get_map(IPRw *, int, const IPFlowID &, Packet *)
{
    return 0;
}

//
// IPRw
//

IPRw::IPRw()
{
}

IPRw::~IPRw()
{
}

int
IPRw::notify_pattern(Pattern *p, ErrorHandler *)
{
    for (int i = 0; i < _all_patterns.size(); i++)
	if (_all_patterns[i] == p)
	    return 0;
    _all_patterns.push_back(p);
    return 0;
}


int
IPRw::parse_input_spec(const String &line, InputSpec &is,
		       String name, ErrorHandler *errh)
{
    PrefixErrorHandler cerrh(errh, name + ": ");
    String word, rest;
    if (!cp_word(line, &word, &rest))
	return cerrh.error("empty argument");
    cp_eat_space(rest);

    is.kind = INPUT_SPEC_DROP;

    if (word == "pass" || word == "passthrough" || word == "nochange") {
	int32_t outnum = 0;
	if (rest && !cp_integer(rest, &outnum))
	    return cerrh.error("syntax error, expected 'nochange [OUTPUT]'");
	else if (outnum < 0 || outnum >= noutputs())
	    return cerrh.error("output port out of range");
	is.kind = INPUT_SPEC_NOCHANGE;
	is.u.output = outnum;

    } else if (word == "keep") {
	if (cp_va_space_kparse(rest, this, ErrorHandler::silent_handler(),
			       "FOUTPUT", cpkP+cpkM, cpUnsigned, &is.u.pattern.fport,
			       "ROUTPUT", cpkP+cpkM, cpUnsigned, &is.u.pattern.rport,
			       cpEnd) < 0)
	    return cerrh.error("syntax error, expected 'keep FOUTPUT ROUTPUT'");
	if (is.u.pattern.fport >= noutputs() || is.u.pattern.rport >= noutputs())
	    return cerrh.error("output port out of range");
	is.kind = INPUT_SPEC_KEEP;
	is.u.pattern.p = 0;

    } else if (word == "drop" || word == "discard") {
	if (rest)
	    return cerrh.error("syntax error, expected '%s'", word.c_str());

    } else if (word == "pattern") {
	if (Pattern::parse_with_ports(rest, &is.u.pattern.p, &is.u.pattern.fport, &is.u.pattern.rport, this, &cerrh) < 0)
	    return -1;
	if (is.u.pattern.fport >= noutputs() || is.u.pattern.rport >= noutputs())
	    return cerrh.error("output port out of range");
	is.u.pattern.p->use();
	is.kind = INPUT_SPEC_PATTERN;
	if (notify_pattern(is.u.pattern.p, &cerrh) < 0)
	    return -1;

    } else if (Element *e = cp_element(word, this, 0)) {
	IPMapper *mapper = (IPMapper *)e->cast("IPMapper");
	if (rest)
	    return cerrh.error("syntax error, expected 'ELEMENTNAME'");
	else if (!mapper)
	    return cerrh.error("element is not an IPMapper");
	else {
	    is.kind = INPUT_SPEC_MAPPER;
	    is.u.mapper = mapper;
	    mapper->notify_rewriter(this, &cerrh);
	}

    } else
	return cerrh.error("unknown specification");

    return 0;
}


inline IPRw::Mapping *
IPRw::Mapping::free_from_list(Map &map, bool notify)
{
    // see also clear_map below
    //click_chatter("kill %s", reverse()->flow_id().reverse().unparse().c_str());
    Mapping *next = _free_next;
    if (notify && _pat)
	_pat->mapping_freed(primary());
    map.erase(reverse()->flow_id().reverse());
    map.erase(flow_id().reverse());
    delete reverse();
    delete this;
    return next;
}

void
IPRw::take_state_map(Map &map, Mapping **free_head, Mapping **free_tail,
		     const Vector<Pattern *> &in_patterns,
		     const Vector<Pattern *> &out_patterns)
{
    Mapping *to_free = 0;
    int np = in_patterns.size();
    int no = noutputs();

    for (Map::iterator iter = map.begin(); iter.live(); iter++) {
	Mapping *m = iter.value();
	if (m->is_primary()) {
	    Pattern *p = m->pattern(), *q = 0;
	    for (int i = 0; i < np; i++)
		if (in_patterns[i] == p) {
		    q = out_patterns[i];
		    break;
		}
	    if (p)
		p->mapping_freed(m);
	    if (q && m->output() < no && m->reverse()->output() < no) {
		q->accept_mapping(m);
		if (m->free_tracked()) {
		    if (free_head)
			m->append_to_free(*free_head, *free_tail);
		    else
			m->clear_free_tracked();
		}
	    } else {
		m->set_free_next(to_free);
		to_free = m;
	    }
	}
    }

    while (to_free)
	to_free = to_free->free_from_list(map, false);
}

void
IPRw::clean_map(Map &table, uint32_t last_jif)
{
    //click_chatter("cleaning map");
    Mapping *to_free = 0;

    for (Map::iterator iter = table.begin(); iter.live(); iter++)
	if (Mapping *m = iter.value()) {
	    if (m->is_primary() && !m->used_since(last_jif) && !m->free_tracked()) {
		m->set_free_next(to_free);
		to_free = m;
	    }
	}

    while (to_free)
	to_free = to_free->free_from_list(table, true);
}

void
IPRw::clean_map_free_tracked(Map &table,
			     Mapping *&free_head, Mapping *&free_tail,
			     uint32_t last_jif)
{
    Mapping *free_list = free_head;
    Mapping **prev_ptr = &free_list;

    Mapping *m = free_list;
    while (m) {
	Mapping *next = m->free_next();
	if (!m->session_over()) {
	    // reuse of a port; take it off the free-tracked list
	    *prev_ptr = next;
	    m->clear_free_tracked();
	} else if (m->used_since(last_jif))
	    break;
	else
	    prev_ptr = &m->_free_next;
	m = next;
    }

    // cut off free_list before 'm'
    *prev_ptr = 0;

    // move free_head forward, to 'm' or beyond
    if (m && m->free_next()) {
	// if 'm' exists, then shift it to the end of the list
	free_head = m->free_next();
	m->set_free_next(0);
	m->append_to_free(free_head, free_tail);
    } else
	free_head = free_tail = m;

    // free contents of free_list
    while (free_list)
	free_list = free_list->free_from_list(table, true);
}

void
IPRw::incr_clean_map_free_tracked(Map &table,
				  Mapping *&free_head, Mapping *&free_tail,
				  uint32_t last_jif)
{
    Mapping *m = free_head;
    if (!m->session_over()) {
	// has been recycled; remove from free-tracked list
	free_head = m->free_next();
	if (!free_head)
	    free_tail = 0;
	m->clear_free_tracked();
    } else if (m->used_since(last_jif)) {
	// recently used; cycle to end of list
	if (m->free_next()) {
	    free_head = m->free_next();
	    m->set_free_next(0);
	    m->append_to_free(free_head, free_tail);
	}
    } else {
	// actually free; delete it
	free_head = m->free_from_list(table, true);
	if (!free_head)
	    free_tail = 0;
    }
}

void
IPRw::clear_map(Map &table)
{
    Mapping *to_free = 0;

    for (Map::iterator iter = table.begin(); iter.live(); iter++) {
	Mapping *m = iter.value();
	if (m->is_primary()) {
	    m->set_free_next(to_free);
	    to_free = m;
	}
    }

    while (to_free) {
	// don't call free_from_list, because there is no need to update
	// 'table' incrementally
	Mapping *next = to_free->free_next();
	if (Pattern *pat = to_free->pattern())
	    pat->mapping_freed(to_free);
	delete to_free->reverse();
	delete to_free;
	to_free = next;
    }

    table.clear();
}

ELEMENT_PROVIDES(IPRw)
CLICK_ENDDECLS
