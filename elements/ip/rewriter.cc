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
// Rewriter::Connection
//

Rewriter::Connection::Connection()
  : _saddr(), _sport(0), _daddr(), _dport(0), _pat(NULL),
    _used(false), _removed(false)
{
}

Rewriter::Connection::Connection(unsigned long sa, unsigned short sp, 
				 unsigned long da, unsigned short dp)
  : _saddr(sa), _sport(sp), _daddr(da), _dport(dp), _pat(NULL),
    _used(false), _removed(false)
{
}

Rewriter::Connection::Connection(Packet *p)
  : _pat(NULL), _used(false), _removed(false)
{
  click_ip *ip = (click_ip *)p->data();
  _saddr = IPAddress(ip->ip_src.s_addr);
  _daddr = IPAddress(ip->ip_dst.s_addr);

  udphdr *udph = (udphdr *)(p->data() + sizeof(click_ip));
  _sport = udph->uh_sport;	// network byte order
  _dport = udph->uh_dport;	// network byte order
}

Rewriter::Connection
Rewriter::Connection::rev() const
{
  Connection c(_daddr.saddr(), _dport, _saddr.saddr(), _sport);
  c._pat = _pat;
  return c;
}

void
Rewriter::Connection::fix_csums(Packet *p)
{
  unsigned char buf[2048];

  click_ip *ip = (click_ip *)p->data();

  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *)ip, ip->ip_hl << 2);

  int len = ntohs(ip->ip_len);
  memcpy(buf, p->data(), p->length());
  memset(buf, '\0', 9);		// set up pseudoheader

  click_ip *iph = (click_ip *)buf;
  iph->ip_sum = htons(len - sizeof(click_ip));
  if (ip->ip_p == IP_PROTO_TCP) {
    tcp_header *tcph = (tcp_header *)(buf + sizeof(click_ip));
    tcph->th_sum = 0;
    tcp_header *realth = (tcp_header *)(p->data() + sizeof(click_ip));
    realth->th_sum = in_cksum(buf, len);
  } else {
    udphdr *udph = (udphdr *)(buf + sizeof(click_ip));
    udph->uh_sum = 0;
    udphdr *realuh = (udphdr *)(p->data() + sizeof(click_ip));
    realuh->uh_sum = in_cksum(buf, len);
  }
}

void
Rewriter::Connection::apply(Packet *p)
{
  click_ip *iph = (click_ip *)p->data();
  iph->ip_src.s_addr = _saddr.saddr();
  iph->ip_dst.s_addr = _daddr.saddr();

  udphdr *udph = (udphdr *)(p->data() + sizeof(click_ip));
  udph->uh_sport = _sport;
  udph->uh_dport = _dport;

  fix_csums(p);
  mark_used();
}

inline 
Rewriter::Connection::operator bool() const
{
  return _saddr || _daddr || _sport || _dport;
}

inline bool
Rewriter::Connection::operator==(Connection &c)
{ 
  return (_saddr == c._saddr && _daddr == c._daddr
	  && _sport == c._sport && _dport == c._dport);
}

inline unsigned
Rewriter::Connection::hashcode(void) const
{ 
#define CHUCK_MAGIC 0x4c6d92b3;
#define ROT(v, r) ((v)<<(r) | ((unsigned)(v))>>(32-(r)))
  return (ROT(_saddr.hashcode(), 13) 
	  ^ ROT(_daddr.hashcode(), 23) ^ (_sport | (_dport<<16)));
}

inline String
Rewriter::Connection::s(void) const
{
  String sport = (String)((int)ntohs(_sport));
  String dport = (String)((int)ntohs(_dport));
  return _saddr.s() + "/" + sport + " -> " + _daddr.s() + "/" + dport;
}

//
// Rewriter::Pattern
//

Rewriter::Pattern::Pattern(void)
  : _saddr(), _sportl(0), _sporth(0), _daddr(), _dport(0), _free(), _output(-1)
{
}

Rewriter::Pattern::Pattern(int o)
  : _saddr(), _sportl(0), _sporth(0), _daddr(), _dport(0), _free(), _output(o)
{
}

inline void
Rewriter::Pattern::init_ports()
{
  int sz = _sporth - _sportl + 1;
  assert(sz > 0);

  _free.resize(sz, 0);
  for (int i = 0; i < sz; i++)
    _free[i] = _sporth-i;
}

bool
Rewriter::Pattern::initialize(String &s)
{
  // Pattern is "saddr sport[-sport2] daddr dport".
  // Any element can be '*'.  If sport2 is present, neither sport nor
  // sport2 is '*', and there is no space around '-'.
  Vector<String> v;
  String r, r2;

  cp_spacevec(s, v);
  if (v.size() != 4)
    return false;
  r = v[0];			// source address
  if (r == "*")
    _saddr = 0;
  else if (!cp_ip_address(r, _saddr))
    return false;
  r = v[1];			// source port
  if (r == "*")
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
  if (r == "*")
    _daddr = 0;
  else if (!cp_ip_address(r, _daddr))
    return false;
  r = v[3];			// destination port
  if (r == "*")
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
Rewriter::Pattern::apply(Connection &in, Connection &out)
{
  out._saddr = _saddr ? _saddr : in._saddr;
  out._daddr = _daddr ? _daddr : in._daddr;
  out._dport = _dport ? htons((short)_dport) : in._dport;

  if (!_sporth) {
    out._sport = in._sport;
    return true;
  } else if (_sporth == _sportl) {
    out._sport = htons((short)_sporth);
    return true;
  } else if (_free.size() > 0) {
    out._sport = htons((short)_free.back());
    _free.pop_back();
    return true;
  }
  return false;
}

inline bool
Rewriter::Pattern::free(Connection &c)
{
  if (!_sporth)
    return false;
  if (_sporth == _sportl)
    return false;
  if ((!_saddr || _saddr == c._saddr)
      && (!_daddr || _daddr == c._daddr)
      && (!_dport || _dport == c._dport)
      && (_sportl <= c._sport && c._sport <= _sporth)) {
    _free.push_back(c._sport);
    return true;
  }
  return false;
}

inline String
Rewriter::Pattern::s(void) const
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
  : _patterns(), _npat(0), _fwd(), _rev(), _timer(this), _mc(NULL)
{
}

Rewriter::~Rewriter()
{
  assert(!_timer.scheduled());
  for (int i = 0; i < _patterns.size(); i++)
    delete _patterns[i];
}

int
Rewriter::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  _npat = args.size();
  if (_npat < 1) {
    errh->error("Rewriter must be configured with at least one pattern");
    return -1;
  }

  set_ninputs(2);
  int base = _npat == 1 ? 0 : 1;
  set_noutputs(1+base+_npat);
  for (int i = 0; i < _npat; i++) {
    Pattern *p = new Pattern(base+i);
    if (!p->initialize(args[i])) {
      errh->error("Argument %d to rewriter (`%s') is an illegal pattern",
		  i, args[i].data());
      return -1;
    }
    _patterns.push_back(p);
  }

  return 0;
}

int
Rewriter::initialize(ErrorHandler *errh)
{
  Vector<Element *> mcs;
  CastElementFilter filter("MappingCreator");
  int ok = router()->downstream_elements(this, 0, &filter, mcs);
  if (ok < 0)
    return errh->error("downstream_elements failure");
  filter.filter(mcs);
  if (mcs.size() == 1)
    _mc = (MappingCreator *)mcs[0];
  else if (_npat > 1)
    return errh->error("Rewriter with multiple patterns must have a "
		       "MappingCreator on output 0[%d]",mcs.size());

  _timer.schedule_after_ms(_gc_interval_sec * 1000);
#if defined(CLICK_LINUXMODULE) && !defined(HAVE_TCP_PROT)
  click_chatter(
       "rewriter: The kernel does not export the symbol `tcp_prot'.\n"
       "          Rewriter therefore cannot remove stale mappings.\n"
       "          Apply the Click kernel patch to fix this problem."
       );
#endif
#ifndef CLICK_LINUXMODULE
  click_chatter("rewriter: Running in userlevel mode.  "
		"Cannot remove stale mappings.");
  click_chatter("Patterns:\n%s", dump_patterns().cc());
#endif

  return 0;
}

void
Rewriter::uninitialize()
{
  _timer.unschedule();
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

void
Rewriter::mark_live_tcp()
{
#ifdef CLICK_LINUXMODULE
#ifdef HAVE_TCP_PROT
  start_bh_atomic();

  for (struct sock *sp = tcp_prot.sklist_next;
       sp != (struct sock *)&tcp_prot;
       sp = sp->sklist_next) {
    Connection c(sp->rcv_saddr, ntohs(sp->sport), 
		 sp->daddr, ntohs(sp->dport));
    Connection out = _fwd[c];
    if (out)
      out.mark_used();
  }

  end_bh_atomic();
#endif
#endif
}

void
Rewriter::clean()
{
  int i = 0;
  Connection in, out;
  while (_fwd.each(i, in, out)) {
    if (!in.used() && !out.used()) {
      Pattern *p = out.pattern();
      if (p)
	p->free(out);
      in.remove();
      out.remove();
    } else {
      in.reset_used();
      out.reset_used();
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

bool
Rewriter::establish_mapping(Packet *p, int npat)
{
  if (npat < 0 || npat >= _npat) {
    click_chatter("rewriter: attempt to make mapping with undefined pattern");
    return false;
  }

  Pattern *pat = _patterns[npat];
  Connection in(p), out;

  if (!pat->apply(in, out))
    return false;
#ifdef debugging
  click_chatter("establishing %s", out.s().cc());
#endif

  out.set_pattern(pat);
  out.set_output(pat->output());
  _fwd.insert(in, out);
  _rev.insert(out.rev(), in.rev());
  
  return true;
}

bool
Rewriter::establish_mapping(Connection &in, Connection &out, int output)
{
#ifdef debugging
  click_chatter("establishing(2) %s", out.s().cc());
#endif
  out.set_pattern(NULL);
  out.set_output(output);
  _fwd.insert(in, out);
  _rev.insert(out.rev(), in.rev());
  
  return true;
}

bool
Rewriter::apply_mapping(Packet *p, int &port)
{
  Connection in(p), out;
  if ((out = _fwd[in])) {
    port = out.output();
    out.apply(p);
#ifdef debugging
    click_chatter("mapping::apply applied %s", out.s().cc());
#endif
    return true;
  } else {
#ifdef debugging
    click_chatter("mapping::apply could not find %s", in.s().cc());
#endif
    return false;
  }
}

bool
Rewriter::rapply_mapping(Packet *p)
{
  Connection rev;
  if ((rev = _rev[Connection(p)])) {
    rev.apply(p);
    return true;
  } else
    return false;
}

void
Rewriter::push(int port, Packet *p)
{
  p = p->uniqueify();		// WHEN USING INFINITESOURCE

  if (port == 0) {
    int oport;
    if (_mc) {
      if (apply_mapping(p, oport))
	output(oport).push(p);
      else
	output(0).push(p);
    } else {
      if (!apply_mapping(p, oport)) {
	if (!establish_mapping(p, 0)) {
	  click_chatter("rewriter: out of mappings (dropping packet)"); 
	  return;
	}
	apply_mapping(p, oport);
      }
      output(oport).push(p);
    }
  } else {
    if (rapply_mapping(p))
      output(noutputs()-1).push(p);
    else
      click_chatter("rewriter: cannot do reverse mapping (dropping packet)");
  }
}

inline String
Rewriter::dump_table()
{
  String s;
  int i = 0;
  Connection in, out;

  while (_fwd.each(i, in, out)) {
    if (i > 1) s += "\n";
    s += in.s() + " ==> " + out.s();
  }
  return s;
}

inline String
Rewriter::dump_patterns()
{
  String s;
  for (int i = 0; i < _patterns.size(); i++)
    s += _patterns[i]->s() + "\n";
  return s;
}

EXPORT_ELEMENT(Rewriter)

#include "hashmap.cc"
template class HashMap<Rewriter::Connection, Rewriter::Connection>;
