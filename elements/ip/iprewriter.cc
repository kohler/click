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
#include "elemfilter.hh"
#include "router.hh"
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

void
IPRewriter::Mapping::make_pair(const IPFlowID &inf, const IPFlowID &outf,
			     Pattern *pattern, int foutput, int routput,
			     Mapping *&in_map, Mapping *&out_map)
{
  in_map = new Mapping(inf, outf, pattern, foutput, false);
  out_map = new Mapping(outf.rev(), inf.rev(), pattern, routput, true);
  in_map->_reverse = out_map;
  out_map->_reverse = in_map;
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

IPRewriter::Pattern::Pattern()
  : _saddr(), _sportl(0), _sporth(0), _daddr(), _dport(0)
{
}

bool
IPRewriter::Pattern::initialize(String &s)
{
  // Pattern is "saddr sport[-sport2] daddr dport".
  // Any element can be '-'.  If sport2 is present, neither sport nor
  // sport2 is '-', and there is no space around '-'.
  Vector<String> v;
  String r, r2;

  cp_spacevec(s, v);
  if (v.size() == 1 && v[0] == "-")
    for (int i = 1; i < 4; i++)
      v.push_back("-");
  
  if (v.size() != 4)
    return false;

  // source address
  if (v[0] == "-")
    _saddr = 0;
  else if (!cp_ip_address(v[0], _saddr))
    return false;

  // source port
  if (v[1] == "-")
    _sportl = _sporth = 0;
  else {
    if (!cp_integer(v[1], 10, _sportl, &r2))
      return false;
    if (r2) {
      if (!cp_integer(r2, 10, _sporth) || _sporth > 0)
	return false;
      _sporth = -_sporth;
    } else
      _sporth = _sportl;
  }
  if (_sportl > _sporth)
    return false;
#if 0
  if (_sportl == 0)
    return false;
#endif

  // destination address
  if (v[2] == "-")
    _daddr = 0;
  else if (!cp_ip_address(v[2], _daddr))
    return false;
  
  // destination port
  if (v[3] == "-")
    _dport = 0;
  else {
    if (!cp_integer(v[3], 10, _dport, &r2))
      return false;
    if (_dport < 0 || _dport > SHRT_MAX)
      return false;
  }
  
  _rover = 0;
  return true;
}

void
IPRewriter::Pattern::clear()
{
  _saddr = _daddr = IPAddress();
  _sportl = _sporth = _dport = 0;
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
      && ((_sportl <= o._sportl && o._sportl <= _sporth)
	  || (o._sportl <= _sportl && _sportl <= o._sporth)))
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
    Mapping *next = _rover->pat_next();
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

inline bool
IPRewriter::Pattern::create_mapping(const IPFlowID &in,
				  int forward_output, int reverse_output,
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
  
  IPFlowID out(_saddr ? _saddr : in.saddr(),
	       new_sport,
	       _daddr ? _daddr : in.daddr(),
	       _dport ? htons((short)_dport) : in.dport());

  Mapping::make_pair(in, out, this, forward_output, reverse_output,
		     forward, reverse);
  forward->pat_insert_after(_rover);
  _rover = forward;
  return true;
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

inline String
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
// IPRewriter
//

IPRewriter::IPRewriter()
  : _patterns(0), _tcp_map(0), _udp_map(0), _timer(this)
{
  set_ninputs(2);
}

IPRewriter::~IPRewriter()
{
  assert(!_timer.scheduled());
}

void
IPRewriter::notify_ninputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

void
IPRewriter::notify_noutputs(int n)
{
  set_noutputs(n < 1 ? 2 : n);
}

int
IPRewriter::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  if (args.size() == 0)
    args.push_back("-");

  _patterns = new Pattern[args.size()];
  _npatterns = args.size();
  int ret = 0;
  for (int i = 0; i < _npatterns; i++)
    if (!_patterns[i].initialize(args[i])) {
      ret = errh->error("illegal pattern in argument %d", i);
      _patterns[i].clear();
    } else {
      for (int j = 0; j < i; j++)
	if (_patterns[j].definite_conflict(_patterns[i]))
	  ret = errh->error("pattern argument %d conflicts with pattern argument %d", i, j);
	else if (_patterns[j].possible_conflict(_patterns[i]))
	  errh->warning("pattern argument %d may conflict with pattern argument %d", i, j);
    }
  
  return ret;
}

int
IPRewriter::initialize(ErrorHandler *errh)
{
  _timer.schedule_after_ms(_gc_interval_sec * 1000);
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
IPRewriter::uninitialize()
{
  _timer.unschedule();
  delete[] _patterns;
  _patterns = 0;
}

void
IPRewriter::mark_live_tcp()
{
#if defined(CLICK_LINUXMODULE) && defined(HAVE_TCP_PROT)
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
}

void
IPRewriter::clean_one_map(HashMap<IPFlowID, Mapping *> &h)
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
  clean_one_map(_tcp_map);
  clean_one_map(_udp_map);
}

void
IPRewriter::run_scheduled()
{
#if defined(CLICK_LINUXMODULE) && defined(HAVE_TCP_PROT)
  mark_live_tcp();
#endif
  clean();
  _timer.schedule_after_ms(_gc_interval_sec * 1000);
}

void
IPRewriter::install(int protocol, Mapping *forward, Mapping *reverse)
{
  IPFlowID forward_flow_id = reverse->flow_id().rev();
  IPFlowID reverse_flow_id = forward->flow_id().rev();
  if (protocol == IP_PROTO_TCP) {
    _tcp_map.insert(forward_flow_id, forward);
    _tcp_map.insert(reverse_flow_id, reverse);
  } else {
    _udp_map.insert(forward_flow_id, forward);
    _udp_map.insert(reverse_flow_id, reverse);
  }
}

IPRewriter::Mapping *
IPRewriter::establish_mapping(Packet *p, int npat, int output)
{
  if (npat < 0 || npat >= _npatterns) {
    click_chatter("IPRewriter: attempt to make mapping with undefined pattern");
    return 0;
  }

  Pattern *pat = &_patterns[npat];
  Mapping *forward, *reverse;
  IPFlowID2 flow(p);
  if (pat->create_mapping(flow, output, noutputs() - 1, forward, reverse)) {
    install(flow.protocol(), forward, reverse);
    return forward;
  } else
    return 0;
}

IPRewriter::Mapping *
IPRewriter::establish_mapping(const IPFlowID2 &flow, const IPFlowID &new_flow,
			      int output)
{
  Mapping *forward, *reverse;
  Mapping::make_pair(flow, new_flow, 0, output, noutputs() - 1,
		     forward, reverse);
  install(flow.protocol(), forward, reverse);
#ifdef debugging
  click_chatter("establishing(2) %s", out.s().cc());
#endif
  return forward;
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
    if (port == 1)
      /* nada */;
    else if (!_patterns[0]) {	// optimize common case
      output(0).push(p);
      return;
    } else
      m = establish_mapping(p, 0, 0);
    if (!m) {
      p->kill();
      return;
    }
  }
  
  m->apply(p);
  output(m->output()).push(p);
}


inline String
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

inline String
IPRewriter::dump_patterns()
{
  String s;
  for (int i = 0; i < _npatterns; i++)
    s += _patterns[i].s() + "\n";
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
