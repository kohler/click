/*
 * iprewriter.{cc,hh} -- rewrites packet source and destination
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "iprewriter.hh"
#include "click_ip.h"
#include "click_tcp.h"
#include "click_udp.h"
#include "confparse.hh"
#include "error.hh"

#include <limits.h>

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
// IPRewriter::Mapping
//

IPRewriter::Mapping::Mapping(const IPFlowID &in, const IPFlowID &out,
			     Pattern *pat, int output, bool is_reverse)
  : _mapto(out), _out(output), _used(false), _is_reverse(is_reverse),
    _pat(pat), _pat_prev(0), _pat_next(0)
{
  // set checksum increments
  const unsigned short *source_words = (const unsigned short *)&in;
  const unsigned short *dest_words = (const unsigned short *)&_mapto;
  unsigned increment = 0;
  for (int i = 0; i < 4; i++) {
    increment += (~ntohs(source_words[i]) & 0xFFFF);
    increment += ntohs(dest_words[i]);
  }
  while (increment >> 16)
    increment = (increment & 0xFFFF) + (increment >> 16);
  _ip_csum_incr = increment;
  
  for (int i = 4; i < 6; i++) {
    increment += (~ntohs(source_words[i]) & 0xFFFF);
    increment += ntohs(dest_words[i]);
  }
  while (increment >> 16)
    increment = (increment & 0xFFFF) + (increment >> 16);
  _udp_csum_incr = increment;
}

bool
IPRewriter::Mapping::make_pair(const IPFlowID &inf, const IPFlowID &outf,
			       Pattern *pattern, int foutput, int routput,
			       Mapping *&in_map, Mapping *&out_map)
{
  in_map = new Mapping(inf, outf, pattern, foutput, false);
  if (!in_map)
    return false;
  out_map = new Mapping(outf.rev(), inf.rev(), pattern, routput, true);
  if (!out_map) {
    delete in_map;
    return false;
  }
  in_map->_reverse = out_map;
  out_map->_reverse = in_map;
  return true;
}

void
IPRewriter::Mapping::apply(Packet *p)
{
  click_ip *iph = p->ip_header();
  assert(iph);
  
  // IP header
  iph->ip_src = _mapto.saddr();
  iph->ip_dst = _mapto.daddr();

  unsigned sum = (~ntohs(iph->ip_sum) & 0xFFFF) + _ip_csum_incr;
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);
  iph->ip_sum = ~htons(sum);

  // UDP/TCP header
  if (iph->ip_p == IP_PROTO_TCP) {
    click_tcp *tcph = (click_tcp *)((unsigned *)iph + iph->ip_hl);
    tcph->th_sport = _mapto.sport();
    tcph->th_dport = _mapto.dport();
    unsigned sum2 = (~ntohs(tcph->th_sum) & 0xFFFF) + _udp_csum_incr;
    while (sum2 >> 16)
      sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);
    tcph->th_sum = ~htons(sum2);
  } else {
    click_udp *udph = (click_udp *)((unsigned *)iph + iph->ip_hl);
    udph->uh_sport = _mapto.sport();
    udph->uh_dport = _mapto.dport();
    if (udph->uh_sum) {		// 0 checksum is no checksum
      unsigned sum2 = (~ntohs(udph->uh_sum) & 0xFFFF) + _udp_csum_incr;
      while (sum2 >> 16)
	sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);
      udph->uh_sum = ~htons(sum2);
    }
  }
  
  mark_used();
}

inline void
IPRewriter::Mapping::pat_unlink()
{
  _pat_next->_pat_prev = _pat_prev;
  _pat_prev->_pat_next = _pat_next;
}

//
// IPRewriter::Pattern
//

IPRewriter::Pattern::Pattern(const IPAddress &saddr, int sportl, int sporth,
			     const IPAddress &daddr, int dport,
			     int forward_output, int reverse_output)
  : _saddr(saddr), _sportl(sportl), _sporth(sporth), _daddr(daddr),
    _dport(dport), _forward_output(forward_output),
    _reverse_output(reverse_output), _rover(0)
{
}

IPRewriter::Pattern *
IPRewriter::Pattern::make(const String &conf, ErrorHandler *errh)
{
  Vector<String> words;
  cp_spacevec(conf, words);
  if (words.size() != 6) {
    errh->error("bad pattern spec: expected `SADDR SPORT[-SPORT2] DADDR DPORT FORWARD-OUTPUT REVERSE-OUTPUT'");
    return 0;
  }

  IPAddress saddr, daddr;
  int sportl, sporth, dport, foutput, routput;
  
  if (words[0] == "-")
    saddr = 0;
  else if (!cp_ip_address(words[0], saddr)) {
    errh->error("bad source address `%s' in pattern spec", words[0].cc());
    return 0;
  }

  if (words[1] == "-")
    sportl = sporth = 0;
  else {
    String rest;
    if (!cp_integer(words[1], &sportl, &rest)) {
      errh->error("bad source port `%s' in pattern spec", words[1].cc());
      return 0;
    }
    if (rest) {
      if (!cp_integer(rest, &sporth) || sporth > 0) {
	errh->error("bad source port `%s' in pattern spec", words[1].cc());
	return 0;
      }
      sporth = -sporth;
    } else
      sporth = sportl;
  }
  if (sportl > sporth || sportl < 0 || sporth > USHRT_MAX) {
    errh->error("source port(s) %d-%d out of range in pattern spec", sportl, sporth);
    return 0;
  }

  if (words[2] == "-")
    daddr = 0;
  else if (!cp_ip_address(words[2], daddr)) {
    errh->error("bad destination address `%s' in pattern spec", words[2].cc());
    return 0;
  }
  
  if (words[3] == "-")
    dport = 0;
  else if (!cp_integer(words[3], &dport)) {
    errh->error("bad destination port `%s' in pattern spec", words[3].cc());
    return 0;
  }
  if (dport < 0 || dport > USHRT_MAX) {
    errh->error("destination port %d out of range in pattern spec", dport);
    return 0;
  }

  if (!cp_integer(words[4], &foutput) || foutput < 0) {
    errh->error("bad forward output `%s' in pattern spec", words[4].cc());
    return 0;
  }
  
  if (!cp_integer(words[5], &routput) || routput < 0) {
    errh->error("bad reverse output `%s' in pattern spec", words[5].cc());
    return 0;
  }
  
  return new Pattern(saddr, sportl, sporth, daddr, dport, foutput, routput);
}

static bool
possible_conflict_port(IPAddress a1, int p1l, int p1h,
		       IPAddress a2, int p2l, int p2h)
{
  if (a1 && a2 && a1 != a2)
    return false;
  if (!p1l || !p2l)
    return true;
  if ((p1l <= p2l && p2l <= p1h) || (p2l <= p1l && p1l <= p2h))
    return true;
  return false;
}

bool
IPRewriter::Pattern::possible_conflict(const Pattern &o) const
{
  return possible_conflict_port(_saddr, _sportl, _sporth,
				o._saddr, o._sportl, o._sporth)
    && possible_conflict_port(_daddr, _dport, _dport,
			      o._daddr, o._dport, o._dport);
}

bool
IPRewriter::Pattern::definite_conflict(const Pattern &o) const
{
  if (_saddr && _sportl && _daddr && _dport
      && _saddr == o._saddr && _daddr == o._daddr && _dport == o._dport
      && ((_sportl <= o._sportl && o._sporth <= _sporth)
	  || (o._sportl <= _sportl && _sporth <= o._sporth)))
    return true;
  else
    return false;
}

inline unsigned short
IPRewriter::Pattern::find_sport()
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
IPRewriter::Pattern::create_mapping(const IPFlowID &in,
				    Mapping *&forward, Mapping *&reverse)
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

  if (Mapping::make_pair(in, out, this, _forward_output, _reverse_output,
			 forward, reverse)) {
    forward->pat_insert_after(_rover);
    _rover = forward;
    return true;
  } else
    return false;
}

inline void
IPRewriter::Pattern::mapping_freed(Mapping *m)
{
  if (_rover == m) {
    _rover = m->pat_next();
    if (_rover == m)
      _rover = 0;
  }
  m->pat_unlink();
}

bool
IPRewriter::Pattern::accept_mapping(const IPFlowID &in, const IPFlowID &out,
				    Mapping *&forward, Mapping *&reverse)
{
  if ((in.saddr() == out.saddr() && !_saddr)
      || out.saddr() == _saddr) {
    if ((in.sport() == out.sport() && !_sportl)
	|| out.sport() >= _sportl && out.sport() <= _sporth) {
      if ((in.daddr() == out.daddr() && !_daddr)
	  || out.daddr() == _daddr) {
	if ((in.dport() == out.dport() && !_dport)
	    || out.dport() == _dport)
	  return true;
      }
    }
  }

  unsigned short want_sport = out.sport();
  if (_rover) {
    Mapping *r = _rover;
    Mapping *p = 0;
    do {
      if (r->sport() == want_sport)
	return false;
      else if (r->sport() < want_sport && (!p || p->sport() < r->sport()))
	p = r;
      r = r->pat_next();
    } while (r != _rover);
    _rover = p;
  }

  if (Mapping::make_pair(in, out, this, _forward_output, _reverse_output,
			 forward, reverse)) {
    forward->pat_insert_after(_rover);
    _rover = forward;
    return true;
  } else
    return false;
}

String
IPRewriter::Pattern::s() const
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
IPMapper::mapper_patterns(Vector<IPRewriter::Pattern *> &, IPRewriter *) const
{
}

IPRewriter::Mapping *
IPMapper::get_map(bool, const IPFlowID &, IPRewriter *)
{
  return 0;
}

//
// IPRewriter
//

IPRewriter::IPRewriter()
  : _tcp_map(0), _udp_map(0), _timer(this)
{
}

IPRewriter::~IPRewriter()
{
  assert(!_timer.scheduled());
}

void
IPRewriter::notify_noutputs(int n)
{
  set_noutputs(n < 1 ? 1 : n);
}

void
IPRewriter::collect_patterns(Vector<Pattern *> &pv, Vector<int> &sv)
{
  for (int i = 0; i < _input_specs.size(); i++)
    switch (_input_specs[i].kind) {
      
     case INPUT_SPEC_PATTERN:
      pv.push_back(_input_specs[i].u.pattern);
      sv.push_back(i);
      break;
      
     case INPUT_SPEC_MAPPER:
      _input_specs[i].u.mapper->mapper_patterns(pv, this);
      sv.resize(pv.size(), i);
      break;
      
     default:
      break;
      
    }
}

int
IPRewriter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() == 0)
    return errh->error("too few arguments; expected `IPRewriter(INPUTSPEC, ...)'");
  set_ninputs(conf.size());

  // parse arguments
  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    String word, rest;
    if (!cp_word(conf[i], &word, &rest)) {
      errh->error("input %d spec is empty", i);
      continue;
    }
    cp_eat_space(rest);

    InputSpec is;
    is.kind = INPUT_SPEC_DROP;
    
    if (word == "nochange") {
      int outnum = 0;
      if ((rest && !cp_integer(rest, &outnum))
	  || (outnum < 0 || outnum >= noutputs()))
	errh->error("bad input %d spec; expected `nochange [OUTPUT]'", i);
      is.kind = INPUT_SPEC_NOCHANGE;
      is.u.output = outnum;
      
    } else if (word == "drop") {
      if (rest)
	errh->error("bad input %d spec; expected `drop'", i);

    } else if (word == "pattern") {
      if (Pattern *p = Pattern::make(rest, errh)) {
	is.kind = INPUT_SPEC_PATTERN;
	is.u.pattern = p;
      }

    } else if (Element *e = cp_element(word, this, 0)) {
      IPMapper *mapper = (IPMapper *)e->cast("IPMapper");
      if (rest || !mapper)
	errh->error("bad input %d spec; expected `ELEMENTNAME'", i);
      else {
	is.kind = INPUT_SPEC_MAPPER;
	is.u.mapper = mapper;
      }
      
    } else
      errh->error("unknown input %d spec `%s'", i, word.cc());

    _input_specs.push_back(is);
  }

  // check patterns for conflicts
  Vector<Pattern *> all_pat;
  Vector<int> all_pat_source;
  collect_patterns(all_pat, all_pat_source);
  for (int i = 0; i < all_pat.size(); i++)
    for (int j = 0; j < i; j++) {
      if (all_pat[j]->definite_conflict(*all_pat[i]))
	errh->error("pattern conflict");
      else if (all_pat[j]->possible_conflict(*all_pat[i]))
	errh->warning("possible pattern conflict");
      else
	continue;
      String s = all_pat[i]->s(), t = all_pat[j]->s();
      errh->message("  pattern from input spec %d: %s", all_pat_source[i], s.cc());
      errh->message("  pattern from input spec %d: %s", all_pat_source[j], t.cc());
    }

  // check patterns for bad output numbers
  for (int i = 0; i < all_pat.size(); i++) {
    int f = all_pat[i]->forward_output(), r = all_pat[i]->reverse_output();
    if (f < 0 || f >= noutputs())
      errh->error("pattern from input spec %d: bad forward output `%d'", all_pat_source[i], f);
    if (r < 0 || r >= noutputs())
      errh->error("pattern from input spec %d: bad reverse output `%d'", all_pat_source[i], r);
  }
  
  return (errh->nerrors() == before ? 0 : -1);
}

int
IPRewriter::initialize(ErrorHandler *errh)
{
  _timer.attach(this);
  _timer.schedule_after_ms(GC_INTERVAL_SEC * 1000);
#if defined(CLICK_LINUXMODULE) && !defined(HAVE_TCP_PROT)
  errh->message
    ("The kernel does not export the symbol `tcp_prot', so I cannot remove\n"
     "stale mappings. Apply the Click kernel patch to fix this problem.");
#endif
#ifndef CLICK_LINUXMODULE
  errh->message("can't remove stale mappings at userlevel");
  click_chatter("Patterns:\n%s", dump_patterns().cc());
#endif

  return 0;
}

void
IPRewriter::take_map_state(bool is_tcp,
			   const HashMap<IPFlowID, Mapping *> &omap,
			   const Vector<Pattern *> &all_pat,
			   const Vector<int> &, ErrorHandler *errh)
{
  bool warned = false;
  
  int i = 0;
  IPFlowID flow;
  Mapping *m;
  while (omap.each(i, flow, m))
    if (m && m->is_forward()) {
      Mapping *new_f = 0, *new_r = 0;
      for (int j = 0; j < all_pat.size() && !new_f; j++)
	all_pat[j]->accept_mapping(flow, m->flow_id(), new_f, new_r);
      if (new_f)
	install(is_tcp, new_f, new_r);
      else if (!warned) {
	errh->warning("not all old %s mappings could be transferred",
		      (is_tcp ? "TCP" : "UDP"));
	warned = true;
      }
    }
}

void
IPRewriter::take_state(Element *e, ErrorHandler *errh)
{
  IPRewriter *rw = (IPRewriter *)e->cast("IPRewriter");
  if (!rw) return;

  Vector<Pattern *> all_pat;
  Vector<int> all_pat_source;
  collect_patterns(all_pat, all_pat_source);
  take_map_state(false, rw->_udp_map, all_pat, all_pat_source, errh);
  take_map_state(true, rw->_tcp_map, all_pat, all_pat_source, errh);
}

void
IPRewriter::uninitialize()
{
  _timer.unschedule();

  clear_map(_tcp_map);
  clear_map(_udp_map);

  for (int i = 0; i < _input_specs.size(); i++)
    if (_input_specs[i].kind == INPUT_SPEC_PATTERN)
      delete _input_specs[i].u.pattern;
}

void
IPRewriter::mark_live_tcp()
{
#if defined(CLICK_LINUXMODULE) && defined(HAVE_TCP_PROT)
#if 0
  start_bh_atomic();

  for (struct sock *sp = tcp_prot.sklist_next;
       sp != (struct sock *)&tcp_prot;
       sp = sp->sklist_next) {
    // socket port numbers are already in network byte order
    IPFlowID c(sp->rcv_saddr, sp->sport, sp->daddr, sp->dport);
    if (Mapping *m = _tcp_map.find(c))
      m->mark_used();
  }

  end_bh_atomic();
#endif
#endif
}

void
IPRewriter::clear_map(HashMap<IPFlowID, Mapping *> &h)
{
  Vector<Mapping *> to_free;
  
  int i = 0;
  IPFlowID flow;
  Mapping *m;
  while (h.each(i, flow, m))
    if (m && m->is_forward())
      to_free.push_back(m);

  for (i = 0; i < to_free.size(); i++) {
    Mapping *m = to_free[i];
    if (Pattern *p = m->pattern())
      p->mapping_freed(m);
    delete m->reverse();
    delete m;
  }
}

void
IPRewriter::clean_map(HashMap<IPFlowID, Mapping *> &h)
{
  Vector<Mapping *> to_free;
  
  int i = 0;
  IPFlowID flow;
  Mapping *m;
  while (h.each(i, flow, m))
    if (m) {
      if (!m->used() && !m->reverse()->used() && !m->is_reverse())
	to_free.push_back(m);
      else
	m->clear_used();
    }
  
  for (i = 0; i < to_free.size(); i++) {
    m = to_free[i];
    if (Pattern *p = m->pattern())
      p->mapping_freed(m);
    h.insert(m->reverse()->flow_id().rev(), 0);
    h.insert(m->flow_id().rev(), 0);
    delete m->reverse();
    delete m;
  }
}

void
IPRewriter::clean()
{
  clean_map(_tcp_map);
  clean_map(_udp_map);
}

void
IPRewriter::run_scheduled()
{
#if defined(CLICK_LINUXMODULE) && defined(HAVE_TCP_PROT)
  mark_live_tcp();
#endif
  clean();
  _timer.schedule_after_ms(GC_INTERVAL_SEC * 1000);
}

void
IPRewriter::install(bool is_tcp, Mapping *forward, Mapping *reverse)
{
  IPFlowID forward_flow_id = reverse->flow_id().rev();
  IPFlowID reverse_flow_id = forward->flow_id().rev();
  if (is_tcp) {
    _tcp_map.insert(forward_flow_id, forward);
    _tcp_map.insert(reverse_flow_id, reverse);
  } else {
    _udp_map.insert(forward_flow_id, forward);
    _udp_map.insert(reverse_flow_id, reverse);
  }
}

void
IPRewriter::push(int port, Packet *p)
{
  p = p->uniqueify();
  IPFlowID flow(p);
  click_ip *iph = p->ip_header();
  assert(iph->ip_p == IP_PROTO_TCP || iph->ip_p == IP_PROTO_UDP);
  bool tcp = iph->ip_p == IP_PROTO_TCP;

  Mapping *m = (tcp ? _tcp_map.find(flow) : _udp_map.find(flow));
  
  if (!m) {			// create new mapping
    const InputSpec &is = _input_specs[port];
    switch (is.kind) {

     case INPUT_SPEC_NOCHANGE:
      output(is.u.output).push(p);
      return;

     case INPUT_SPEC_DROP:
      break;

     case INPUT_SPEC_PATTERN: {
       Pattern *pat = is.u.pattern;
       Mapping *reverse;
       if (pat->create_mapping(flow, m, reverse))
	 install(tcp, m, reverse);
       break;
     }

     case INPUT_SPEC_MAPPER:
      m = is.u.mapper->get_map(tcp, flow, this);
      break;
      
    }
    if (!m) {
      p->kill();
      return;
    }
  }
  
  m->apply(p);
  output(m->output()).push(p);
}


String
IPRewriter::dump_table()
{
  String tcps, udps;
  int i = 0;
  IPFlowID2 in;
  Mapping *m;
  while (_tcp_map.each(i, in, m))
    if (m && !m->is_reverse())
      tcps += in.s() + " ==> " + m->flow_id().s() + "\n";
  i = 0;
  while (_udp_map.each(i, in, m))
    if (m && !m->is_reverse())
      udps += in.s() + " ==> " + m->flow_id().s() + "\n";
  if (tcps && udps)
    return "TCP:\n" + tcps + "\nUDP:\n" + udps;
  else if (tcps)
    return "TCP:\n" + tcps;
  else if (udps)
    return "UDP:\n" + udps;
  else
    return String();
}

String
IPRewriter::dump_patterns()
{
  String s;
  for (int i = 0; i < _input_specs.size(); i++)
    if (_input_specs[i].kind == INPUT_SPEC_PATTERN)
      s += _input_specs[i].u.pattern->s() + "\n";
  return s;
}

static String
table_dump(Element *f, void *)
{
  IPRewriter *r = (IPRewriter *)f;
  return r->dump_table();
}

static String
patterns_dump(Element *f, void *)
{
  IPRewriter *r = (IPRewriter *)f;
  return r->dump_patterns();
}

void
IPRewriter::add_handlers()
{
  add_read_handler("rewrite_table", table_dump, (void *)0);
  add_read_handler("patterns", patterns_dump, (void *)0);
}

EXPORT_ELEMENT(IPRewriter)

#include "hashmap.cc"
#include "vector.cc"
