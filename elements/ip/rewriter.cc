/*
 * rewriter.{cc,hh} -- rewrites packet source and destination
 * Max Poletto
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
#include "rewriter.hh"
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

//#define debugging

//
// Rewriter::Mapping
//

Rewriter::Mapping::Mapping()
  : _pat(0), _out(0), _used(false), _removed(false)
{
}

Rewriter::Mapping::Mapping(const IPFlowID &in, const IPFlowID &out,
			   Pattern *pat, int output)
  : _mapto(out), _pat(pat), _out(output), _used(false), _removed(false)
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
  _udp_csum_incr = (increment + (increment >> 16)) & 0xFFFF;
}

void
Rewriter::Mapping::apply(Packet *p)
{
  click_ip *iph = (click_ip *)p->data();
  
  // IP header
  iph->ip_src = _mapto.saddr();
  iph->ip_dst = _mapto.daddr();

  unsigned sum = (~ntohs(iph->ip_sum) & 0xFFFF) + _ip_csum_incr;
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);
  iph->ip_sum = ~htons(sum);

  // UDP/TCP header
  click_udp *udph = (click_udp *)(p->data() + sizeof(click_ip));
  udph->uh_sport = _mapto.sport();
  udph->uh_dport = _mapto.dport();

  if (iph->ip_p == IP_PROTO_TCP) {
    click_tcp *tcph = (click_tcp *)(iph + 1);
    unsigned sum2 = (~ntohs(tcph->th_sum) & 0xFFFF) + _udp_csum_incr;
    while (sum2 >> 16)
      sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);
    tcph->th_sum = ~htons(sum2);
  } else {
    click_udp *udph = (click_udp *)(iph + 1);
    if (udph->uh_sum) {		// 0 checksum is no checksum
      unsigned sum2 = (~ntohs(udph->uh_sum) & 0xFFFF) + _udp_csum_incr;
      while (sum2 >> 16)
	sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);
      udph->uh_sum = ~htons(sum2);
    }
  }
  
  mark_used();
}

//
// Rewriter::Pattern
//

Rewriter::Pattern::Pattern()
  : _saddr(), _sportl(0), _sporth(0), _daddr(), _dport(0), _free()
{
}

inline void
Rewriter::Pattern::init_ports()
{
  int sz = _sporth - _sportl + 1;
  assert(sz > 0);

  _free.resize(sz, 0);
  for (int i = 0; i < sz; i++)
    _free[i] = _sporth - i;
}

bool
Rewriter::Pattern::initialize(String &s)
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
  
  r = v[0];			// source address
  if (r == "-")
    _saddr = 0;
  else if (!cp_ip_address(r, _saddr))
    return false;
  
  r = v[1];			// source port
  if (r == "-")
    _sportl = _sporth = 0;
  else {
    if (!cp_integer(r, 10, _sportl, &r2))
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
  
  r = v[2];			// destination address
  if (r == "-")
    _daddr = 0;
  else if (!cp_ip_address(r, _daddr))
    return false;
  
  r = v[3];			// destination port
  if (r == "-")
    _dport = 0;
  else {
    if (!cp_integer(r, 10, _dport, &r2))
      return false;
    if (_dport < 0 || _dport > SHRT_MAX)
      return false;
  }
  
  init_ports();
  return true;
}

inline bool
Rewriter::Pattern::apply(const IPFlowID &in, IPFlowID &out)
{
  bool ok = true;
  unsigned short new_sport;
  if (!_sporth)
    new_sport = in.sport();
  else if (_sporth == _sportl)
    new_sport = htons((short)_sporth);
  else if (_free.size() > 0) {
    new_sport = htons((short)_free.back());
    _free.pop_back();
  } else {
    new_sport = out.sport();
    ok = false;
  }

  out = IPFlowID(_saddr ? _saddr : in.saddr(), new_sport,
		 _daddr ? _daddr : in.daddr(),
		 _dport ? htons((short)_dport) : in.dport());
  return ok;
}

inline bool
Rewriter::Pattern::free(const IPFlowID &c)
{
  if (!_sporth)
    return false;
  if (_sporth == _sportl)
    return false;
  if ((!_saddr || _saddr == c.saddr())
      && (!_daddr || _daddr == c.daddr())
      && (!_dport || _dport == c.dport())
      && (_sportl <= c.sport() && c.sport() <= _sporth)) {
    _free.push_back(c.sport());
    return true;
  }
  return false;
}

inline String
Rewriter::Pattern::s() const
{
  String saddr, sport, daddr, dport;
  saddr = _saddr ? _saddr.s() : String("*");
  daddr = _daddr ? _daddr.s() : String("*");
  dport = _dport ? (String)_dport : String("*");
  if (!_sporth)
    sport = "*";
  else if (_sporth == _sportl)
    sport = String(_sporth);
  else
    sport = String(_sportl) + "-" + String(_sporth);
  return saddr + ":" + sport + " / " + daddr + ":" + dport;
}

//
// Rewriter
//

Rewriter::Rewriter()
  : _patterns(), _timer(this)
{
  set_ninputs(2);
}

Rewriter::~Rewriter()
{
  assert(!_timer.scheduled());
  for (int i = 0; i < _patterns.size(); i++)
    delete _patterns[i];
}

void
Rewriter::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 2 : n);
}

int
Rewriter::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  if (args.size() == 0)
    args.push_back("-");

  for (int i = 0; i < args.size(); i++) {
    Pattern *p = new Pattern;
    if (!p->initialize(args[i])) {
      errh->error("argument %d is an illegal pattern", i);
      return -1;
    }
    _patterns.push_back(p);
  }

  return 0;
}

int
Rewriter::initialize(ErrorHandler *errh)
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
Rewriter::uninitialize()
{
  _timer.unschedule();
}

void
Rewriter::mark_live_tcp()
{
#ifdef CLICK_LINUXMODULE
#ifdef HAVE_TCP_PROT
  start_bh_atomic();

  for (struct sock *sp = tcp_prot.sklist_next;
       sp != (struct sock *)&tcp_prot;
       sp = sp->sklist_next) {
    IPFlowID c(sp->rcv_saddr, ntohs(sp->sport), 
	       sp->daddr, ntohs(sp->dport));
    if (Mapping *out = _tcp_fwd.findp(c))
      out->mark_used();
  }

  end_bh_atomic();
#endif
#endif
}

void
Rewriter::clean()
{
  int i = 0;
  IPFlowID *in;
  Mapping *out;
  while (_tcp_fwd.eachp(i, in, out)) {
    // XXX `in' has no notion of `used'
    if (!out->used()) {
      if (Pattern *p = out->pattern())
	p->free(out->flow_id());
      out->remove();
    } else {
      out->reset_used();
    }
  }
  // XXX?
  i = 0;
  while (_udp_fwd.eachp(i, in, out)) {
    // XXX `in' has no notion of `used'
    if (!out->used()) {
      if (Pattern *p = out->pattern())
	p->free(out->flow_id());
      out->remove();
    } else {
      out->reset_used();
    }
  }
}

void
Rewriter::run_scheduled()
{
#if defined(CLICK_LINUXMODULE) && defined(HAVE_TCP_PROT)
#if 0
  mark_live_tcp();
  clean();
  _timer.schedule_after_ms(_gc_interval_sec * 1000);
#endif
#endif
}

void
Rewriter::establish_mapping(const IPFlowID2 &in, const IPFlowID &out,
			    Pattern *pat, int output)
{
  if (in.protocol() == IP_PROTO_TCP) {
    _tcp_fwd.insert(in, Mapping(in, out, pat, output));
    _tcp_rev.insert(out.rev(), Mapping(out.rev(), in.rev(), pat, -1));
  } else if (in.protocol() == IP_PROTO_UDP) {
    _udp_fwd.insert(in, Mapping(in, out, pat, output));
    _udp_rev.insert(out.rev(), Mapping(out.rev(), in.rev(), pat, -1));
  }
}

bool
Rewriter::establish_mapping(Packet *p, int npat, int output)
{
  if (npat < 0 || npat >= _patterns.size()) {
    click_chatter("rewriter: attempt to make mapping with undefined pattern");
    return false;
  }

  Pattern *pat = _patterns[npat];
  IPFlowID2 in(p);
  IPFlowID out;
  
  if (!pat->apply(in, out))
    return false;
#ifdef debugging
  click_chatter("establishing %s", out.s().cc());
#endif
  
  establish_mapping(in, out, pat, output);
  return true;
}

bool
Rewriter::establish_mapping(const IPFlowID2 &in, const IPFlowID &out,
			    int output)
{
#ifdef debugging
  click_chatter("establishing(2) %s", out.s().cc());
#endif
  establish_mapping(in, out, 0, output);
  return true;
}

void
Rewriter::push(int port, Packet *p)
{
  // XXX what to really do with a packet that is not TCP or UDP?
  click_ip *iph = p->ip_header();
  if (iph->ip_p != IP_PROTO_TCP && iph->ip_p != IP_PROTO_UDP) {
    p->kill();
    return;
  }

  p = p->uniqueify();		// WHEN USING INFINITESOURCE
  IPFlowID flow(p);
  bool tcp = p->ip_header()->ip_p == IP_PROTO_TCP;

  if (port == 0) {
    Mapping *m = (tcp ? _tcp_fwd.findp(flow) : _udp_fwd.findp(flow));
    
    if (!m) {			// create new mapping
      if (!*_patterns[0])	// optimize common case
	output(0).push(p);
      else if (establish_mapping(p, 0, 0))
	m = (tcp ? _tcp_fwd.findp(flow) : _udp_fwd.findp(flow));
      else {
	click_chatter("failed to establish mapping!");
	p->kill();}
      if (!m)
	return;
    }
    m->apply(p);
    output(m->output()).push(p);
    
  } else {
    Mapping *m = (tcp ? _tcp_rev.findp(flow) : _udp_rev.findp(flow));
    if (m) {
      m->apply(p);
      output(noutputs() - 1).push(p);
    } else {
      click_chatter("rewriter: cannot do reverse mapping (dropping packet)");
      p->kill();
    }
  }
}


inline String
Rewriter::dump_table()
{
  String tcps, udps;
  int i = 0;
  IPFlowID2 in;
  Mapping out;
  while (_tcp_fwd.each(i, in, out))
    tcps += in.s() + " ==> " + out.flow_id().s() + "\n";
  i = 0;
  while (_udp_fwd.each(i, in, out))
    udps += in.s() + " ==> " + out.flow_id().s() + "\n";
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
Rewriter::dump_patterns()
{
  String s;
  for (int i = 0; i < _patterns.size(); i++)
    s += _patterns[i]->s() + "\n";
  return s;
}

static String
table_dump(Element *f, void *)
{
  Rewriter *r = (Rewriter *)f;
  return r->dump_table();
}

static String
patterns_dump(Element *f, void *)
{
  Rewriter *r = (Rewriter *)f;
  return r->dump_patterns();
}

void
Rewriter::add_handlers()
{
  add_read_handler("rewrite_table", table_dump, (void *)0);
  add_read_handler("patterns", patterns_dump, (void *)0);
}

EXPORT_ELEMENT(Rewriter)

#include "hashmap.cc"
template class HashMap<IPFlowID, IPFlowID>;
template class HashMap<IPFlowID, Rewriter::Mapping>;
