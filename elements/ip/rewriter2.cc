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

// todo: should pattern::apply ever fail with sport=*?

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "rewriter2.hh"
#include "click_ip.h"
#include "click_tcp.h"
#include "click_udp.h"
#include "confparse.hh"
#include "error.hh"

extern "C" {
#include <asm/softirq.h>
#include <net/sock.h>
#ifdef HAVE_TCP_PROT
extern struct proto tcp_prot;
#endif
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

inline char *
Rewriter::Pattern::eat_ws(char *s)
{
  while (*s && isspace(*s)) ++s;
  return s;
}

bool
Rewriter::Pattern::initialize(const char *s)
{
  String *r;

  s = eat_ws(s);

  if (*s == '*')		// source address
    _saddr = 0;
  else if (cp_ip_address(String(s), _saddr, r))
    s = r->cc();
  else return false;

  if (*s != ' ') return false;
  s = eat_ws(s);

  if (*s == '*')		// source port
    _sportl = _sporth = 0;
  else {
    char *e;
    long int v = strtol(s, &e, 10);
    if (s == e || *e != ' ' && *e != '-')
      return false;
    _sportl = (int)v;
    s = eat_ws(e);
    if (*s == '-') {
      s = eat_ws(s+1);
      v = strtol(s, &e, 10);
      if (s == e || *e != ' ')
	return false;
      _sporth = (int)v;
      s = e;
    } else
      _sporth = _sportl;
  }

  s = eat_ws(s);
  if (*s == '*')		// destination address
    _daddr = 0;
  else if (cp_ip_address(String(s), _daddr, r))
    s = r->cc();
  else return false;

  if (*s != ' ') return false;
  s = eat_ws(s);

  if (*s == '*') {		// source port
    _dport = 0; s++;
  } else {
    char *e;
    long int v = strtol(s, &e, 10);
    if (s == e || *e != ' ' && *e != '\0')
      return false;
    _dport = (int)v;
    s = e;
  }

  s = eat_ws(s);
  if (*s) return false;

  init_ports();
  return true;
}

inline bool
Rewriter::Pattern::apply(Connection *in, Connection *out)
{
  if (_free.size() > 0) {
    out->_saddr = _saddr ? _saddr : in->_saddr;
    out->_daddr = _daddr ? _daddr : in->_daddr;
    out->_dport = _dport ? _dport : in->_dport;
    int sport = _free[_free.size()-1];
    _free.pop_back();
    out->_sport = sport ? sport : in->_sport;
    return true;
  }
  return false;
}

inline bool
Rewriter::Pattern::free(Connection *c)
{
  if ((!_saddr || _saddr == c->_saddr)
      && (!_daddr || _daddr == c->_daddr)
      && (!_dport || _dport == c->_dport)
      && (_sportl <= c->_sport && c->_sport <= _sporth)) {
    _free.push_back(c->_sport);
    return true;
  }
  return false;
}

//
// Rewriter::Connection
//

Rewriter::Connection::Connection()
  : _saddr(), _sport(0), _daddr(), _dport(0), _used(false), _removed(false)
{
}

Rewriter::Connection::Connection(unsigned long sa, unsigned short sp, 
				 unsigned long da, unsigned short dp)
  : _saddr(sa), _sport(sp), _daddr(da), _dport(dp), 
    _used(false), _removed(false)
{
}

Rewriter::Connection::Connection(Packet *p)
  : _used(false), _removed(false)
{
  click_ip *ip = p->ip_header();
  _saddr = IPAddress(ip->ip_src.s_addr);
  _daddr = IPAddress(ip->ip_dst.s_addr);

  udphdr *udph = (udphdr *)(p->data() + sizeof(click_ip));
  _sport = udph->uh_sport;
  _dport = udph->uh_dport;
}

void
Rewriter::Connection::fix_csums(Packet *p)
{
  click_ip *ip = p->ip_header();

  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *)ip, ip->ip_hl << 2);

  int len = ntohs(ip->ip_len);
  unsigned char *buf = new unsigned char[p->length()];
  memcpy(buf, p->data(), p->length());
  memset(buf, '\0', 9);	// set up pseudoheader

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
  iph->ip_src.s_addr = htonl(_saddr.saddr());
  iph->ip_dst.s_addr = htonl(_daddr.saddr());

  udphdr *udph = (udphdr *)(p->data() + sizeof(click_ip));
  udph->uh_sport = htons(_sport);
  udph->uh_dport = htons(_dport);

  fix_csums(p);

  _used = true;
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
  return _saddr.s() + "/" + _sport + " -> " + _daddr.s() + "/" + _dport;
}

//
// Rewriter::Mapping
//

Rewriter::Mapping::Mapping()
  : _fwd(), _rev()
{
}

bool
Rewriter::Mapping::add(Packet *p, Pattern *pat)
{
  Connection *in = new Connection(p);
  Connection *out = new Connection();
  if (!pat->apply(in, out))
    return false;
  _fwd.insert(*in, Rewrite(out,pat));
  _rev.insert(*out, *in);
  return true;
}

bool
Rewriter::Mapping::apply(Packet *p, int *port)
{
  Rewrite out;
  if ((out = _fwd[Connection(p)])) {
    *port = out.pattern()->output();
    out.connection()->apply(p);
    return true;
  } else
    return false;
}

bool
Rewriter::Mapping::rapply(Packet *p)
{
  Connection c;
  if ((c = _rev[Connection(p)])) {
    c.apply(p);
    return true;
  } else
    return false;
}

void
Rewriter::Mapping::mark_live_tcp()
{
#ifdef HAVE_TCP_PROT
  start_bh_atomic();

  for (struct sock *sp = tcp_prot.sklist_next;
       sp != (struct sock *)&tcp_prot;
       sp = sp->sklist_next) {
    Connection c(sp->rcv_saddr, ntohs(sp->sport), 
		 sp->daddr, ntohs(sp->dport));
    Rewrite r = _fwd[c];
    if (r)
      r.connection()->mark_used();
  }

  end_bh_atomic();
#endif
}

void
Rewriter::Mapping::clean()
{
  int i = 0;
  Connection c1;
  Rewrite r;
  while (_fwd.each(i, c1, r)) {
    Pattern *p = r.pattern();
    Connection *c2 = r.connection();
    if (!c1.used() && !c2->used()) {
      p->free(&c1);
      c1.remove();
      c2->remove();
    } else {
      c1.reset_used();
      c2->reset_used();
    }
  }
}

String
Rewriter::Mapping::s()
{
  String s = "";
  int i = 0;
  Connection c1;
  Rewrite r;

  while (_fwd.each(i, c1, r)) {
    s += c1.s() + " ==> " + r.connection()->s() + "\n";
  }
  return s;
}

//
// Rewriter
//

Rewriter::Rewriter()
  : _timer(this), _patterns()
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
    const char *s = args[i].data();
    Pattern *p = new Pattern(base+i);
    if (!p->initialize(s)) {
      errh->error("Argument %d to rewriter (`%s') is an illegal pattern");
      return -1;
    }
    _patterns.push_back(p);
  }

  return 0;
}

static String
rewriter_dump(Element *f, void *v)
{
  Rewriter *r = (Rewriter *)f;
  return r->dump_table();
}

void
Rewriter::add_handlers()
{
  add_read_handler("rewrite_table", rewriter_dump, (void *)0);
}

int
Rewriter::initialize(ErrorHandler *errh)
{
  _timer.schedule_after_ms(_gc_interval_sec * 1000);

#ifndef HAVE_TCP_PROT
  click_chatter(
       "rewriter: warning: The kernel does not export the symbol `tcp_prot'.\n"
       "                   Rewriter will not garbage collect stale mappings.\n"
       "                   Apply the Click kernel patch to fix this problem."
       );
#endif

  return 0;
}

void
Rewriter::uninitialize()
{
  _timer.unschedule();
}

void
Rewriter::check_tcp()
{
}

void
Rewriter::run_scheduled()
{
#ifdef HAVE_TCP_PROT
  _mapping.mark_live_tcp();
  _mapping.clean();
  _timer.schedule_after_ms(_gc_interval_sec * 1000);
#endif
}

bool
Rewriter::establish_mapping(Packet *p, int pat)
{
  if (pat < 0 || pat >= _npat) {
    click_chatter("rewriter: attempt to make mapping with undefined pattern");
    return false;
  }
  return _mapping.add(p, _patterns[pat]);
}

void
Rewriter::push(int port, Packet *p)
{
  if (port == 0) {
    int oport;
    if (_npat == 1) {
      if (!_mapping.apply(p, &oport)) {
	if (!establish_mapping(p, 0)) {
	  click_chatter("rewriter: out of mappings! (dropping packet)"); 
	  return;
	}
	_mapping.apply(p, &oport);
      }
      output(oport).push(p);
    } else {
      if (_mapping.apply(p, &oport))
	output(oport).push(p);
      else
	output(0).push(p);
    }
  } else {
    if (_mapping.rapply(p))
      output(noutputs()-1).push(p);
    else
      click_chatter("rewriter: dropping unknown pattern in reverse mapping");
  }
}

inline String
Rewriter::dump_table()
{
  return _mapping.s();
}

//EXPORT_ELEMENT(Rewriter)
ELEMENT_REQUIRES(linuxmodule)

#include "hashmap.cc"
template class HashMap<Rewriter::Connection, Rewriter::Rewrite>;
template class HashMap<Rewriter::Connection, Rewriter::Connection>;
