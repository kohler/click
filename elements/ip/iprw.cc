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
#include <click/integers.hh>	// for first_bit_set

#ifdef CLICK_LINUXMODULE
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <asm/softirq.h>
#include <net/sock.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
#endif


//
// IPRw::Mapping
//

IPRw::Mapping::Mapping(bool dst_anno)
  : _is_reverse(false), /* _used(false), */ _marked(false),
    _flow_over(false), _free_tracked(false), _dst_anno(dst_anno),
    _ip_p(0), _pat(0), _pat_prev(0), _pat_next(0), _free_next(0)
{
}

void
IPRw::Mapping::initialize(int ip_p, const IPFlowID &in, const IPFlowID &out,
			  int output, bool is_reverse, Mapping *reverse)
{
  // set fields
  _ip_p = ip_p;
  _mapto = out;
  _output = output;
  assert(output >= 0 && output < 256);
  _is_reverse = is_reverse;
  _reverse = reverse;
  
  // set checksum deltas
  const unsigned short *source_words = (const unsigned short *)&in;
  const unsigned short *dest_words = (const unsigned short *)&_mapto;
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
  in_map->initialize(ip_p, inf, outf, foutput, false, out_map);
  out_map->initialize(ip_p, outf.rev(), inf.rev(), routput, true, in_map);
}

void
IPRw::Mapping::apply(WritablePacket *p)
{
  click_ip *iph = p->ip_header();
  assert(iph);
  
  // IP header
  iph->ip_src = _mapto.saddr();
  iph->ip_dst = _mapto.daddr();
  if (_dst_anno)
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
    unsigned sum2 = (~tcph->th_sum & 0xFFFF) + _udp_csum_delta;
    sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);
    tcph->th_sum = ~(sum2 + (sum2 >> 16));
    
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
    if (udph->uh_sum) {		// 0 checksum is no checksum
      unsigned sum2 = (~udph->uh_sum & 0xFFFF) + _udp_csum_delta;
      sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);
      udph->uh_sum = ~(sum2 + (sum2 >> 16));
    }
    
  }
}

String
IPRw::Mapping::s() const
{
  return reverse()->flow_id().rev().s() + " => " + flow_id().s() + " [" + String(output()) + "]";
}

//
// IPRw::Pattern
//

IPRw::Pattern::Pattern(const IPAddress &saddr, int sportl, int sporth,
		       const IPAddress &daddr, int dport)
  : _saddr(saddr), _sportl(sportl), _sporth(sporth), _daddr(daddr),
    _dport(dport), _rover(0), _is_napt(true), _refcount(0), _nmappings(0)
{
}

int
IPRw::Pattern::parse_napt(Vector<String> &words, Pattern **pstore,
			  Element *e, ErrorHandler *errh)
{
  // otherwise, pattern definition
  if (words.size() != 4)
    return errh->error("bad pattern spec: should be `NAME' or `SADDR SPORT DADDR DPORT'");
  
  IPAddress saddr, daddr;
  int32_t sportl, sporth, dport;
  
  if (words[0] == "-")
    saddr = 0;
  else if (!cp_ip_address(words[0], &saddr, e))
    return errh->error("bad source address `%s' in pattern spec", words[0].cc());
  
  if (words[1] == "-")
    sportl = sporth = 0;
  else {
    int dash = words[1].find_left('-');
    if (dash < 0) dash = words[1].length();
    if (!cp_integer(words[1].substring(0, dash), &sportl))
      return errh->error("bad source port `%s' in pattern spec", words[1].cc());
    if (dash < words[1].length()) {
      if (!cp_integer(words[1].substring(dash + 1), &sporth))
	return errh->error("bad source port `%s' in pattern spec", words[1].cc());
    } else
      sporth = sportl;
  }
  if (sportl > sporth || sportl < 0 || sporth > 0xFFFF)
    return errh->error("source port(s) %d-%d out of range in pattern spec", sportl, sporth);

  if (words[2] == "-")
    daddr = 0;
  else if (!cp_ip_address(words[2], &daddr, e))
    return errh->error("bad destination address `%s' in pattern spec", words[2].cc());
  
  if (words[3] == "-")
    dport = 0;
  else if (!cp_integer(words[3], &dport))
    return errh->error("bad destination port `%s' in pattern spec", words[3].cc());
  if (dport < 0 || dport > 0xFFFF)
    return errh->error("destination port %d out of range in pattern spec", dport);

  *pstore = new Pattern(saddr, sportl, sporth, daddr, dport);
  return 0;
}

int
IPRw::Pattern::parse_nat(Vector<String> &words, Pattern **pstore,
			 Element *e, ErrorHandler *errh)
{
  // otherwise, pattern definition
  if (words.size() != 2)
    return errh->error("bad pattern spec: should be `NAME' or `SADDR/PREFIX DADDR'");
  
  IPAddress saddr1, saddr2;  
  if (words[0] == "-")
    saddr1 = saddr2 = 0;
  else if (cp_ip_address(words[0], &saddr1, e))
    saddr2 = saddr1;
  else if (cp_ip_prefix(words[0], &saddr1, &saddr2, e)) {
    unsigned s1 = saddr1 & saddr2;
    unsigned s2 = saddr1 | ~saddr2;
    // don't count xxxx.0 and xxxx.255
    saddr1 = htonl(ntohl(s1) + 1);
    saddr2 = htonl(ntohl(s2) - 1);
  } else {
    int dash = words[0].find_left('-');
    if (dash >= 0
	&& cp_ip_address(words[0].substring(0, dash), &saddr1, e)
	&& cp_ip_address(words[0].substring(dash+1), &saddr2, e))
      /* ok */;
    else
      return errh->error("bad source address `%s' in pattern spec", words[0].cc());
  }

  // check that top 16 bits agree
  int sportl = 0, sporth = 0;
  if (saddr1 != saddr2) {
      // gcc-2.96 unhappy with combined expression
      uint32_t xorval = ntohl(saddr1.addr()) ^ ntohl(saddr2.addr());
      int first_different = first_bit_set(xorval);
      if (first_different <= 16)
	  return errh->error("source addresses `%s' and `%s' too far apart;\nmust agree in at least top 16 bits", saddr1.s().cc(), saddr2.s().cc());
      IPAddress prefix = ~IPAddress::make_prefix(first_different - 1);
      sportl = ntohl((unsigned)(saddr1 & prefix));
      sporth = ntohl((unsigned)(saddr2 & prefix));
      if (sportl > sporth)
	  return errh->error("lower source address should come first");
      saddr1 &= ~prefix;
  }

  IPAddress daddr;
  if (words[1] == "-")
    daddr = 0;
  else if (!cp_ip_address(words[1], &daddr, e))
    return errh->error("bad destination address `%s' in pattern spec", words[2].cc());
  
  *pstore = new Pattern(saddr1, sportl, sporth, daddr, 0);
  (*pstore)->_is_napt = false;
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
    String name = cp_unquote(words[0]);
    if (Pattern *p = IPRewriterPatterns::find(e, name, errh)) {
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
  return (_saddr == o._saddr
	  && _daddr == o._daddr
	  && _dport == o._dport
	  && _sportl <= o._sportl
	  && o._sporth <= _sporth);
}

inline unsigned short
IPRw::Pattern::find_sport()
{
  if (_sportl == _sporth || !_rover)
    return _sportl;

  // search for empty port number starting at `_rover'
  Mapping *r = _rover;
  uint16_t this_sport = ntohs(r->sport());
  do {
    Mapping *next = r->pat_next();
    uint16_t next_sport = ntohs(next->sport());
    if (next_sport > this_sport + 1)
      goto found;
    else if (next_sport <= this_sport) {
      if (this_sport < _sporth)
	goto found;
      else if (next_sport > _sportl) {
	this_sport = _sportl - 1;
	goto found;
      }
    }
    r = next;
    this_sport = next_sport;
  } while (r != _rover);

  // nothing found
  return 0;

 found:
  _rover = r;
  return this_sport + 1;
}

bool
IPRw::Pattern::create_mapping(int ip_p, const IPFlowID &in,
			      int fport, int rport,
			      Mapping *fmap, Mapping *rmap)
{
  uint16_t new_sport = 0;
  if (_sportl) {
    new_sport = find_sport();
    if (!new_sport)
      return false;
  }
  
  IPFlowID out(in);

  if (_saddr) {
    if (_is_napt || _sportl == _sporth)
      out.set_saddr(_saddr);
    else {
      assert(ip_p == 0);
      unsigned new_saddr = ntohl((unsigned)_saddr);
      new_saddr |= new_sport;
      out.set_saddr(htonl(new_saddr));
    }
  }

  if (_sportl && _is_napt)
    out.set_sport(htons(new_sport));

  if (_daddr)
    out.set_daddr(_daddr);

  if (_dport) {
    uint16_t new_dport = htons((short)_dport);
    out.set_dport(new_dport);
  }

  Mapping::make_pair(ip_p, in, out, fport, rport, fmap, rmap);
  fmap->pat_insert_after(this, _rover);
  _rover = fmap;
  return true;
}

void
IPRw::Pattern::accept_mapping(Mapping *fmap)
{
  fmap->pat_insert_after(this, _rover);
  _rover = fmap;
}

inline void
IPRw::Pattern::mapping_freed(Mapping *m)
{
  if (_rover == m) {
    _rover = m->pat_next();
    if (_rover == m)
      _rover = 0;
  }
  m->pat_unlink();
}

String
IPRw::Pattern::s() const
{
  StringAccum sa;
  if (!_is_napt && _sportl != _sporth)
      sa << IPAddress(htonl(ntohl(_saddr.addr()) + _sportl)).s() << '-'
	 << IPAddress(htonl(ntohl(_saddr.addr()) + _sporth)).s();
  else if (_saddr)
      sa << _saddr.s();
  else
      sa << '-';

  if (!_is_napt)
      /* nada */;
  else if (!_sporth)
      sa << " -";
  else if (_sportl == _sporth)
      sa << ' ' << _sporth;
  else
      sa << ' ' << _sportl << '-' << _sporth;

  if (_daddr)
      sa << ' ' << _daddr.s();
  else
    sa << " -";

  if (!_is_napt)
      /* nada */;
  else if (!_dport)
      sa << " -";
  else
      sa << ' ' << _dport;

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
    MOD_INC_USE_COUNT;
}

IPRw::~IPRw()
{
    MOD_DEC_USE_COUNT;
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
  String word, rest;
  if (!cp_word(line, &word, &rest))
    return errh->error("%s is empty", name.cc());
  cp_eat_space(rest);
  
  is.kind = INPUT_SPEC_DROP;
  
  if (word == "nochange" || word == "passthru" || word == "passthrough") {
    int32_t outnum = 0;
    if (rest && !cp_integer(rest, &outnum))
      return errh->error("%s: syntax error; expected `nochange [OUTPUT]'", name.cc());
    else if (outnum < 0 || outnum >= noutputs())
      return errh->error("%s: output port out of range", name.cc());
    is.kind = INPUT_SPEC_NOCHANGE;
    is.u.output = outnum;
    
  } else if (word == "keep") {
    if (cp_va_parse(rest, this, ErrorHandler::silent_handler(),
		    cpUnsigned, "forward output", &is.u.keep.fport,
		    cpUnsigned, "reverse output", &is.u.keep.rport,
		    0) < 0)
      return errh->error("%s: syntax error; expected `keep FOUTPUT ROUTPUT'", name.cc());
    if (is.u.keep.fport >= noutputs() || is.u.keep.rport >= noutputs())
      return errh->error("%s: output port out of range", name.cc());
    is.kind = INPUT_SPEC_KEEP;
    
  } else if (word == "drop") {
    if (rest)
      return errh->error("%s: syntax error; expected `drop'", name.cc());
    
  } else if (word == "pattern") {
    if (Pattern::parse_with_ports(rest, &is.u.pattern.p, &is.u.pattern.fport, &is.u.pattern.rport, this, errh) < 0)
      return -1;
    if (is.u.pattern.fport >= noutputs() || is.u.pattern.rport >= noutputs())
      return errh->error("%s: output port out of range", name.cc());
    is.u.pattern.p->use();
    is.kind = INPUT_SPEC_PATTERN;
    if (notify_pattern(is.u.pattern.p, errh) < 0)
      return -1;
    
  } else if (Element *e = cp_element(word, this, 0)) {
    IPMapper *mapper = (IPMapper *)e->cast("IPMapper");
    if (rest)
      return errh->error("%s: syntax error: expected `ELEMENTNAME'", name.cc());
    else if (!mapper)
      return errh->error("%s: element is not an IPMapper", name.cc());
    else {
      is.kind = INPUT_SPEC_MAPPER;
      is.u.mapper = mapper;
      mapper->notify_rewriter(this, errh);
    }
    
  } else
    return errh->error("%s: unknown specification `%s'", name.cc(), word.cc());
  
  return 0;
}


inline IPRw::Mapping *
IPRw::Mapping::free_from_list(Map &map, bool notify)
{
    // see also clear_map below
    //click_chatter("kill %s", reverse()->flow_id().rev().s().cc());
    Mapping *next = _free_next;
    if (notify && _pat)
	_pat->mapping_freed(primary());
    map.remove(reverse()->flow_id().rev());
    map.remove(flow_id().rev());
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
  
  for (Map::Iterator iter = map.first(); iter; iter++) {
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

    for (Map::Iterator iter = table.first(); iter; iter++)
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

    for (Map::Iterator iter = table.first(); iter; iter++) {
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

#include <click/bighashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class BigHashMap<IPFlowID, IPRw::Mapping *>;
template class BigHashMapIterator<IPFlowID, IPRw::Mapping *>;
template class Vector<IPRw::InputSpec>;
#endif
