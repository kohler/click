/*
 * iprw.{cc,hh} -- rewrites packet source and destination
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "iprw.hh"
#include "elements/ip/iprwpatterns.hh"
#include "click_ip.h"
#include "click_tcp.h"
#include "click_udp.h"
#include "confparse.hh"
#include "straccum.hh"
#include "error.hh"

#ifdef CLICK_LINUXMODULE
extern "C" {
#include <asm/softirq.h>
#include <net/sock.h>
#ifdef HAVE_TCP_PROT
extern struct proto tcp_prot;
#endif
}
#endif


//
// IPRw::Mapping
//

IPRw::Mapping::Mapping()
  : _used(false), _pat(0), _pat_prev(0), _pat_next(0)
{
}

void
IPRw::Mapping::initialize(const IPFlowID &in, const IPFlowID &out,
			  int output, bool is_reverse, Mapping *reverse)
{
  // set fields
  _mapto = out;
  _output = output;
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
IPRw::Mapping::make_pair(const IPFlowID &inf, const IPFlowID &outf,
			 int foutput, int routput,
			 Mapping *in_map, Mapping *out_map)
{
  in_map->initialize(inf, outf, foutput, false, out_map);
  out_map->initialize(outf.rev(), inf.rev(), routput, true, in_map);
}

void
IPRw::Mapping::apply(WritablePacket *p)
{
  click_ip *iph = p->ip_header();
  assert(iph);
  
  // IP header
  iph->ip_src = _mapto.saddr();
  iph->ip_dst = _mapto.daddr();

  unsigned sum = (~iph->ip_sum & 0xFFFF) + _ip_csum_delta;
  sum = (sum & 0xFFFF) + (sum >> 16);
  iph->ip_sum = ~(sum + (sum >> 16));

  // UDP/TCP header
  if (iph->ip_p == IP_PROTO_TCP) {
    click_tcp *tcph = reinterpret_cast<click_tcp *>(p->transport_header());
    tcph->th_sport = _mapto.sport();
    tcph->th_dport = _mapto.dport();
    unsigned sum2 = (~tcph->th_sum & 0xFFFF) + _udp_csum_delta;
    sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);
    tcph->th_sum = ~(sum2 + (sum2 >> 16));
  } else {
    click_udp *udph = reinterpret_cast<click_udp *>(p->transport_header());
    udph->uh_sport = _mapto.sport();
    udph->uh_dport = _mapto.dport();
    if (udph->uh_sum) {		// 0 checksum is no checksum
      unsigned sum2 = (~udph->uh_sum & 0xFFFF) + _udp_csum_delta;
      sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);
      udph->uh_sum = ~(sum2 + (sum2 >> 16));
    }
  }
  
  mark_used();
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
    _dport(dport), _rover(0), _refcount(0)
{
}

int
IPRw::Pattern::parse(const String &conf, Pattern **pstore,
		     Element *e, ErrorHandler *errh)
{
  Vector<String> words;
  cp_spacevec(conf, words);

  // check for IPRewriterPatterns reference
  if (words.size() == 1) {
    if (Pattern *p = IPRewriterPatterns::find(e, cp_unquote(words[0]), errh)) {
      *pstore = p;
      return 0;
    } else
      return -1;
  }

  // otherwise, pattern definition
  if (words.size() != 4)
    return errh->error("bad pattern spec: should be `NAME FOUTPUT ROUTPUT' or\n`SADDR SPORT DADDR DPORT FOUTPUT ROUTPUT'");
  
  IPAddress saddr, daddr;
  int sportl, sporth, dport;
  
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
IPRw::Pattern::parse_with_ports(const String &conf, Pattern **pstore,
				int *fport_store, int *rport_store,
				Element *e, ErrorHandler *errh)
{
  Vector<String> words;
  cp_spacevec(conf, words);
  int fport, rport;

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
    return htons((short)_sportl);

  // search for empty port number starting at `_rover'
  Mapping *r = _rover;
  unsigned short this_sport = ntohs(r->sport());
  do {
    Mapping *next = r->pat_next();
    unsigned short next_sport = ntohs(next->sport());
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
  return htons(this_sport + 1);
}

bool
IPRw::Pattern::create_mapping(const IPFlowID &in, int fport, int rport,
				    Mapping *fmap, Mapping *rmap)
{
  unsigned short new_sport;
  if (!_sportl)
    new_sport = in.sport();
  else {
    new_sport = find_sport();
    if (!new_sport)
      return false;
  }

  // convoluted logic avoids internal compiler errors in gcc-2.95.2
  unsigned short new_dport = (_dport ? htons((short)_dport) : in.dport());
  IPFlowID out(_saddr, new_sport, _daddr, new_dport);
  if (!_saddr) out.set_saddr(in.saddr());
  if (!_daddr) out.set_daddr(in.daddr());

  Mapping::make_pair(in, out, fport, rport, fmap, rmap);
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
  String saddr, sport, daddr, dport;
  saddr = _saddr ? _saddr.s() : String("-");
  daddr = _daddr ? _daddr.s() : String("-");
  dport = _dport ? (String)_dport : String("-");
  if (!_sporth)
    sport = "-";
  else if (_sporth == _sportl)
    sport = String(_sporth);
  else
    sport = String(_sportl) + "-" + String(_sporth);
  return saddr + ":" + sport + " / " + daddr + ":" + dport;
}

//
// IPMapper
//

void
IPMapper::notify_rewriter(IPRw *, ErrorHandler *)
{
}

IPRw::Mapping *
IPMapper::get_map(IPRw *, bool, const IPFlowID &)
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

void
IPRw::notify_pattern(Pattern *p)
{
  for (int i = 0; i < _all_patterns.size(); i++)
    if (_all_patterns[i] == p)
      return;
  _all_patterns.push_back(p);
}


int
IPRw::parse_input_spec(const String &line, InputSpec &is, String name,
		       ErrorHandler *errh)
{
  String word, rest;
  if (!cp_word(line, &word, &rest))
    return errh->error("%s is empty", name.cc());
  cp_eat_space(rest);
  
  is.kind = INPUT_SPEC_DROP;
  
  if (word == "nochange") {
    int outnum = 0;
    if (rest && !cp_integer(rest, &outnum))
      return errh->error("%s: syntax error; expected `nochange [OUTPUT]'", name.cc());
    else if (outnum < 0 || outnum >= noutputs())
      return errh->error("%s: port out of range", name.cc());
    is.kind = INPUT_SPEC_NOCHANGE;
    is.u.output = outnum;
    
  } else if (word == "keep") {
    if (cp_va_parse(rest, this, ErrorHandler::silent_handler(),
		    cpUnsigned, "forward output", &is.u.keep.fport,
		    cpUnsigned, "reverse output", &is.u.keep.rport,
		    0) < 0)
      return errh->error("%s: syntax error; expected `keep FOUTPUT ROUTPUT'", name.cc());
    if (is.u.keep.fport >= noutputs() || is.u.keep.rport >= noutputs())
      return errh->error("%s: port out of range", name.cc());
    is.kind = INPUT_SPEC_KEEP;
    
  } else if (word == "drop") {
    if (rest)
      return errh->error("%s: syntax error; expected `drop'", name.cc());
    
  } else if (word == "pattern") {
    if (Pattern::parse_with_ports(rest, &is.u.pattern.p, &is.u.pattern.fport,
				  &is.u.pattern.rport, this, errh) < 0)
      return -1;
    if (is.u.pattern.fport >= noutputs() || is.u.pattern.rport >= noutputs())
      return errh->error("%s: port out of range", name.cc());
    is.u.pattern.p->use();
    is.kind = INPUT_SPEC_PATTERN;
    notify_pattern(is.u.pattern.p);
    
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


void
IPRw::take_state_map(Map &map, const Vector<Pattern *> &in_patterns,
		     const Vector<Pattern *> &out_patterns)
{
  Vector<Mapping *> to_free;
  int np = in_patterns.size();
  int no = noutputs();

  for (Map::Iterator iter = map.first(); iter; iter++) {
    Mapping *m = iter.value();
    if (m->is_forward()) {
      Pattern *p = m->pattern(), *q = 0;
      for (int i = 0; i < np; i++)
	if (in_patterns[i] == p) {
	  q = out_patterns[i];
	  break;
	}
      if (p)
	p->mapping_freed(m);
      if (q && m->output() < no && m->reverse()->output() < no)
	q->accept_mapping(m);
      else
	to_free.push_back(m);
    }
  }

  for (int i = 0; i < to_free.size(); i++) {
    Mapping *m = to_free[i];
    map.remove(m->reverse()->flow_id().rev());
    map.remove(m->flow_id().rev());
    delete m->reverse();
    delete m;
  }
}

void
IPRw::mark_live_tcp(Map &)
{
#if defined(CLICK_LINUXMODULE) && defined(HAVE_TCP_PROT)
#if 0
  start_bh_atomic();

  for (struct sock *sp = tcp_prot.sklist_next;
       sp != (struct sock *)&tcp_prot;
       sp = sp->sklist_next) {
    // socket port numbers are already in network byte order
    IPFlowID c(sp->rcv_saddr, sp->sport, sp->daddr, sp->dport);
    if (Mapping *m = map.find(c))
      m->mark_used();
  }

  end_bh_atomic();
#endif
#endif
}

void
IPRw::clean_map(Map &table)
{
  Vector<Mapping *> to_free;
  
  for (Map::Iterator iter = table.first(); iter; iter++)
    if (Mapping *m = iter.value()) {
      if (!m->used() && !m->reverse()->used() && !m->is_reverse())
	to_free.push_back(m);
      else
	m->clear_used();
    }
  
  for (int i = 0; i < to_free.size(); i++) {
    Mapping *m = to_free[i];
    if (Pattern *p = m->pattern())
      p->mapping_freed(m);
    table.remove(m->reverse()->flow_id().rev());
    table.remove(m->flow_id().rev());
    delete m->reverse();
    delete m;
  }
}

void
IPRw::clear_map(Map &table)
{
  Vector<Mapping *> to_free;
  
  for (Map::Iterator iter = table.first(); iter; iter++) {
    Mapping *m = iter.value();
    if (m->is_forward())
      to_free.push_back(m);
  }

  for (int i = 0; i < to_free.size(); i++) {
    Mapping *m = to_free[i];
    if (Pattern *p = m->pattern())
      p->mapping_freed(m);
    delete m->reverse();
    delete m;
  }
}

ELEMENT_PROVIDES(IPRw)

#include "bighashmap.cc"
#include "vector.cc"
#if EXPLICIT_TEMPLATE_INSTANCES
template class BigHashMap<IPFlowID, IPRw::Mapping *>;
template class BigHashMapIterator<IPFlowID, IPRw::Mapping *>;
template class Vector<IPRw::InputSpec>;
#endif
