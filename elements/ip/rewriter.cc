/*
 * rewriter.{cc,hh} -- rewrites packet source and destination
 * Max Poletto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#if 0
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "rewriter.hh"
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

static String rewriter_dump(Element *f, void *v);

#define SZ 64

Rewriter::Rewriter()
  : _timer(this), _r2f(0), _f2r(SZ, (Mapping *)0), _free(SZ, 0)
{
  add_input();			// Real->Fake
  add_output();			// Real->Fake
  add_input();			// Fake->Real
  add_output();			// Fake->Real
}

Rewriter::~Rewriter()
{
  assert(!_timer.scheduled());
}

Rewriter *
Rewriter::clone() const
{
  return new Rewriter();
}

int
Rewriter::configure(const String &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpIPAddress, "net for fake addresses", &_fakenet,
			cpIPAddress, "local IP address", &_localaddr,
			cpInteger, "cleanup period (sec)", &_gc_interval_sec,
			cpEnd);
  if (res < 0)
    return res;

  if (_gc_interval_sec <= 0)
    return errh->error("rewriter: cleanup period (sec) must be positive");

  return 0;
}

void
Rewriter::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read("rewrite_table", rewriter_dump, (void *)0);
}

int
Rewriter::initialize(ErrorHandler *errh)
{
  _timer.schedule_after_ms(_gc_interval_sec * 1000);
  int i,j;
  for (i = 1, j = _free.size()-1; i < _f2r.size(); j--, i++) {
    _f2r[i] = 0;
    _free[j] = i;
  }
  _free[j] = -1;

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
  Mapping *m;
  for (int i = 1; i < _f2r.size(); i++) 
    if ((m = _f2r[i]))
      delete m;
}

void
Rewriter::check_tcp()
{
#ifdef HAVE_TCP_PROT
  start_bh_atomic();

  for (struct sock *sp = tcp_prot.sklist_next;
       sp != (struct sock *)&tcp_prot;
       sp = sp->sklist_next) {
    unsigned long dest = sp->daddr;
    if ((dest & 0xff) == _fakenet.saddr()) {
      unsigned long i = ntohl(dest) & 0xffffff;
      Mapping *m;
      if (i <= 0 || i >= _f2r.size() || !(m = _f2r[i])) {
	click_chatter("rewriter: get_index failed (i=%d, sz=%d, %s)",
		      i, _f2r.size(), m ? "m ok":"m is null");
	continue;
      }
      m->used = 1;
    }
  }

  end_bh_atomic();
#endif
}

void
Rewriter::run_scheduled()
{
#ifdef HAVE_TCP_PROT
  check_tcp();
  for (int i = 1; i < _f2r.size(); i++) {
    Mapping *m = _f2r[i];
    if (!m)
      continue;
    if (!m->used) {
      _r2f.insert(IPConnection(m->rs,m->rd), 0);
      _f2r[i] = 0;
      _free.push_back(i);
      delete m;
    } else
      m->used = 0;
  }
  _timer.schedule_after_ms(_gc_interval_sec * 1000);
#endif
}

inline int
Rewriter::next_index()
{
  assert(_free.size() > 0);
  int i = _free[_free.size()-1];
  _free.pop_back();
  return i;
}

inline int
Rewriter::get_index(IPAddress a)
{
  unsigned l = ntohl(a.saddr());
  return l & 0x00ffffff;	// Ignore the top byte 
}

int
Rewriter::fix_csums(Packet *p)
{
  click_ip *ip = p->ip_header();

  ip->ip_sum = 0;
  ip->ip_sum = in_cksum((unsigned char *)ip, ip->ip_hl << 2);
  switch(ip->ip_p) {
  case 1:			// ICMP
    break;
  case 2:			// IGMP
    click_chatter("rewriter: IGMP unimplemented; dropping packet");
    return -1;
  case 6: 			// TCP
  case 17: 			// UDP
    {
      int len = ntohs(ip->ip_len);
      unsigned char *buf = new unsigned char[p->length()];
      memcpy(buf, p->data(), p->length());
      memset(buf, '\0', 9);	// set up pseudoheader
      click_ip *iph = (click_ip *)buf;
      iph->ip_sum = htons(len - sizeof(click_ip));
      if (ip->ip_p == 6) {	// TCP
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
    break;
  default:
    click_chatter("rewriter: unknown protocol type (%d); dropping packet", 
		  ip->ip_p);
    return -1;
  }
  return 0;
}

void
Rewriter::push(int port, Packet *p)
{
  click_ip *ip = p->ip_header();
  IPAddress src(ip->ip_src.s_addr);
  IPAddress dst(ip->ip_dst.s_addr);
  Mapping *m;
  if (port == 0) {		// Real addresses arrive on port 0
    IPConnection conn(src, dst);
    click_chatter("rewriter: push on port 0 [%s]", conn.s().cc());
    int j = _r2f[conn];
    if (j == 0) {		// Need to insert a new entry
      click_chatter("rewriter: creating new mapping");
      int i = next_index();
      if (i < 0) {
	click_chatter("rewriter: out of map space!  Dropping packet.");
	return;
      }
      m = new Mapping;
      m->rs = src; m->rd = dst;
      m->fs = IPAddress(_fakenet.saddr() | htonl(i));
      m->fd = _localaddr;
      m->type = ip->ip_p;
      _r2f.insert(conn, i);
      _f2r[i] = m;
    } else {			// Entry already exists
      m = _f2r[j];
      click_chatter("rewriter: using existing mapping (%d)", j);
    }
    m->used = 1;
    ip->ip_src.s_addr = m->fs.saddr();
    ip->ip_dst.s_addr = m->fd.saddr();
  } else {			// Fake addresses arrive on port 1
    click_chatter("rewriter: push on port 1");
    int i = get_index(dst);
    if (i <= 0 || i >= _f2r.size() || !(m = _f2r[i])) {
      click_chatter("rewriter: get_index failed (i=%d, sz=%d, %s)",
		    i, _f2r.size(), m ? "m ok":"m is null");
      return;
    }
    m = _f2r[i];
    m->used = 1;
    ip->ip_src.s_addr = m->rd.saddr();
    ip->ip_dst.s_addr = m->rs.saddr();
    p->set_dst_ip_anno(m->rs);
  }
  if (fix_csums(p) < 0)		// Need to do this incrementally eventually
       return;
  output(port).push(p);
}

String
Rewriter::dump_table()
{
  String s = "";
  for (int i = 0; i < _f2r.size(); i++) {
    Mapping *m = _f2r[i];
    if (m)
      s += m->rs.s() + "/" + m->rd.s() + " -> " 
	+ m->fs.s() + "/" + m->fd.s() + "\n";
  }
  return s;
}

static String
rewriter_dump(Element *f, void *v)
{
  Rewriter *r = (Rewriter *)f;
  return r->dump_table();
}

//EXPORT_ELEMENT(Rewriter)
//ELEMENT_REQUIRES(linuxmodule)

#include "hashmap.cc"
template class HashMap<IPConnection, int>;
template class Vector<Rewriter::Mapping *>;
#endif
