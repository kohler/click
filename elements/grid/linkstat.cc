/*
 * linkstat.{cc,hh} -- extract per-packet link quality/strength stats
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2002 Massachusetts Institute of Technology
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
#include <click/confparse.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/straccum.hh>
#include <elements/grid/grid.hh>
#include <elements/grid/linkstat.hh>
#include <elements/grid/timeutils.hh>
CLICK_DECLS

LinkStat::LinkStat()
  : _window(100), _tau(10000), _period(1000), 
    _probe_size(1000), _seq(0), _send_timer(0)
{
  MOD_INC_USE_COUNT;
  add_input();
}

LinkStat::~LinkStat()
{
  MOD_DEC_USE_COUNT;
}

LinkStat *
LinkStat::clone() const
{
  return new LinkStat();
}

void
LinkStat::notify_noutputs(int n) 
{
  set_noutputs(n > 0 ? 1 : 0);  
}


int
LinkStat::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"WINDOW", cpUnsigned, "Broadcast loss rate window", &_window,
			"ETH", cpEtherAddress, "Source Ethernet address", &_eth,
			"IP", cpIPAddress, "Source IP address", &_ip,
			"PERIOD", cpUnsigned, "Probe broadcast period (msecs)", &_period,
			"TAU", cpUnsigned, "Loss-rate averaging period (msecs)", &_tau,
			"SIZE", cpUnsigned, "Probe size (bytes)", &_probe_size,
			0);
  if (res < 0)
    return res;
  
  unsigned min_sz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_link_probe);
  if (_probe_size < min_sz)
    return errh->error("Specified packet size is less than the minimum probe size of %u",
		       min_sz);

  return res;
}

void
LinkStat::send_hook() 
{
  WritablePacket *p = Packet::make(_probe_size + 2); // +2 for alignment
  if (p == 0) {
    click_chatter("LinkStat %s: cannot make packet!", id().cc());
    return;
  }
  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  struct timeval tv;
  click_gettimeofday(&tv);
#if 0
  click_chatter("XXX tv = %u.%06u\n", tv.tv_sec, tv.tv_usec);
#endif
  p->set_timestamp_anno(tv);
  
  /* fill in ethernet header */
  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _eth.data(), 6);

  /* fill in the grid header */
  grid_hdr *gh = (grid_hdr *) (eh + 1);
  ASSERT_ALIGNED(gh);
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = htons(_probe_size - sizeof(click_ether));
  gh->type = grid_hdr::GRID_LINK_PROBE;
  gh->ip = gh->tx_ip = _ip;
  
  grid_link_probe *glp = (grid_link_probe *) (gh + 1);
  glp->seq_no = htonl(_seq);
  _seq++;
  glp->period = htonl(_period);
  glp->tau = htonl(_tau);
  
  unsigned min_packet_sz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_link_probe);
  unsigned max_entries = (1500 - min_packet_sz) / sizeof(grid_link_entry);
  unsigned num_entries = max_entries < (unsigned) _bcast_stats.size() ? max_entries : _bcast_stats.size();
  glp->num_links = htonl(num_entries);
  grid_link_entry *e = (grid_link_entry *) (glp + 1);
  for (ProbeMap::const_iterator i = _bcast_stats.begin(); 
       i && num_entries > 0; 
       num_entries--, i++, e++) {
    const probe_list_t &val = i.value();
    if (val.probes.size() == 0) {
      num_entries++;
      continue;
    }
#ifndef SMALL_GRID_PROBES
    e->ip = val.ip;
    e->period = htonl(val.period);
    const probe_t &p = val.probes.back();
    e->last_rx_time = hton(p.when);
    e->last_seq_no = htonl(p.seq_no);
    e->num_rx = htonl(count_rx(&val));
#else
    e->ip = ntohl(val.ip) & 0xff;
    e->num_rx = count_rx(&val);
#endif
  }
  
  unsigned max_jitter = _period / 10;
  long r2 = random();
  unsigned j = (unsigned) ((r2 >> 1) % (max_jitter + 1));
#if 0
  click_chatter("XXX %s:  max_j %u, j %u, -/+ %ld\n", id().cc(), max_jitter, j, r2 & 1);
#endif
  unsigned int delta_us = 1000 * ((r2 & 1) ? _period - j : _period + j);
  _next_bcast.tv_usec += delta_us;
  _next_bcast.tv_sec +=  _next_bcast.tv_usec / 1000000;
  _next_bcast.tv_usec = (_next_bcast.tv_usec % 1000000);
  _send_timer->schedule_at(_next_bcast);
#if 0
  click_chatter("XXX delta = %u, _period = %u\n", delta_ms, _period);
#endif

  checked_output_push(0, p);
}

unsigned int
LinkStat::count_rx(const EtherAddress  &e) 
{
  probe_list_t *pl = _bcast_stats.findp(e);
  if (pl)
    return count_rx(pl);
  else
    return 0;
}

unsigned int
LinkStat::count_rx(const probe_list_t *pl)
{
  if (!pl)
    return 0;

  struct timeval now;
  click_gettimeofday(&now);
  struct timeval period = { _tau / 1000, 1000 * (_tau % 1000) };
  struct timeval earliest = now - period;

  int num = 0;
  for (int i = pl->probes.size() - 1; i >= 0; i--) {
    if (pl->probes[i].when >= earliest)
      num++;
    else
      break;
  }
  return num;
}

unsigned int
LinkStat::count_rx(const IPAddress  &ip) 
{
  for (ProbeMap::const_iterator i = _bcast_stats.begin(); i; i++) {
    if (i.value().ip == (unsigned int) ip) 
      return count_rx(i.key());
  }
  return 0;
}

int
LinkStat::initialize(ErrorHandler *errh)
{
  if (noutputs() > 0) {
    if (!_eth || !_ip) 
      return errh->error("Source IP and Ethernet address must be specified to send probes");
    _send_timer = new Timer(static_send_hook, this);
    _send_timer->initialize(this);
    click_gettimeofday(&_next_bcast);
    _next_bcast.tv_sec++;
    _send_timer->schedule_at(_next_bcast);
  }
  return 0;
}



Packet *
LinkStat::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  EtherAddress ea(eh->ether_shost);

  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("LinkStat %s: got non-Grid packet type", id().cc());
    p->kill();
    return 0;
  }

  grid_hdr *gh = (grid_hdr *) (eh + 1);
  if (gh->type != grid_hdr::GRID_LINK_PROBE) {
    click_chatter("LinkStat %s: got non-Probe packet", id().cc());
    p->kill();
    return 0;
  }

  grid_link_probe *lp = (grid_link_probe *) (gh + 1);
  add_bcast_stat(ea, gh->ip, lp);

  // look in received packet for info about our outgoing link
  struct timeval now;
  click_gettimeofday(&now);
  grid_link_entry *le = (grid_link_entry *) (lp + 1);
  for (unsigned i = 0; i < htonl(lp->num_links); i++, le++) {
#ifdef SMALL_GRID_PROBES
    if ((ntohl(_ip.addr()) & 0xff) == le->ip) {
#else
    if (_ip == le->ip && _period == ntohl(le->period)) {
#endif
      _rev_bcast_stats.insert(EtherAddress(eh->ether_shost), 
			      outgoing_link_entry_t(le, now, ntohl(lp->tau)));
      break;
    }
  }

  p->kill();
  return 0;
}

bool
LinkStat::get_forward_rate(const EtherAddress &e, unsigned int *r, 
			   unsigned int *tau, struct timeval *t)
{
  outgoing_link_entry_t *ol = _rev_bcast_stats.findp(e);
  if (!ol)
    return false;

  if (_period == 0)
    return false;

  unsigned num_expected = ol->tau / _period;
  unsigned num_received = ol->num_rx;

  // will happen if our send period is greater than the the remote
  // host's averaging period
  if (num_expected == 0)
    return false;

  unsigned pct = 100 * num_received / num_expected;
  if (pct > 100)
    pct = 100;
  *r = pct;
  *tau = ol->tau;
  *t = ol->received_at;

  return true;
}

bool
LinkStat::get_reverse_rate(const EtherAddress &e, unsigned int *r, 
			   unsigned int *tau)
{
  probe_list_t *pl = _bcast_stats.findp(e);
  if (!pl)
    return false;

  if (pl->period == 0)
    return false;

  unsigned num_expected = _tau / pl->period;
  unsigned num_received = count_rx(e);

  // will happen if our averaging period is less than the remote
  // host's sending rate.
  if (num_expected == 0)
    return false;

  unsigned pct = 100 * num_received / num_expected;
  if (pct > 100)
    pct = 100;
  *r = pct;
  *tau = _tau;

  return true;
}

void
LinkStat::add_bcast_stat(const EtherAddress &e, const IPAddress &ip, const grid_link_probe *lp)
{
  struct timeval now;
  click_gettimeofday(&now);
  probe_t probe(now, ntohl(lp->seq_no));
  
  unsigned int new_period = ntohl(lp->period);
  
  probe_list_t *l = _bcast_stats.findp(e);
  if (!l) {
    probe_list_t l2(ip, new_period, ntohl(lp->tau));
    _bcast_stats.insert(e, l2);
    l = _bcast_stats.findp(e);
  }
  else if (l->period != new_period) {
    click_chatter("LinkStat %s: node %s has changed its link probe period from %u to %u; clearing probe info\n",
		  id().cc(), ip.s().cc(), l->period, new_period);
    l->probes.clear();
    return;
  }
  
  l->probes.push_back(probe);

  /* only keep stats for last _window *unique* sequence numbers */
  if ((unsigned) l->probes.size() > _window) {
    Vector<probe_t> new_vec;
    for (int i = l->probes.size() - _window; i < l->probes.size(); i++) 
      new_vec.push_back(l->probes.at(i));
  
    l->probes = new_vec;  
  }
}

String
LinkStat::read_window(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;
  return String(f->_window) + "\n";
}

String
LinkStat::read_period(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;
  return String(f->_period) + "\n";
}

String
LinkStat::read_tau(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;
  return String(f->_tau) + "\n";
}

String
LinkStat::read_bcast_stats(Element *xf, void *)
{
  LinkStat *e = (LinkStat *) xf;

  typedef BigHashMap<EtherAddress, bool> EthMap;
  EthMap eth_addrs;
  
  for (ProbeMap::const_iterator i = e->_bcast_stats.begin(); i; i++) 
    eth_addrs.insert(i.key(), true);
  for (ReverseProbeMap::const_iterator i = e->_rev_bcast_stats.begin(); i; i++)
    eth_addrs.insert(i.key(), true);

  struct timeval now;
  click_gettimeofday(&now);

  StringAccum sa;
  for (EthMap::const_iterator i = eth_addrs.begin(); i; i++) {
    const EtherAddress &eth = i.key();
    
    probe_list_t *pl = e->_bcast_stats.findp(eth);
    outgoing_link_entry_t *ol = e->_rev_bcast_stats.findp(eth);
    
    IPAddress ip(pl ? pl->ip : (ol ? ol->ip : 0));

    sa << eth.s() << ' ' << ip << ' ';
    
    sa << "fwd ";
    if (ol) {
      struct timeval age = now - ol->received_at;
      sa << "age=" << age << " tau=" << ol->tau << " num_rx=" << (unsigned) ol->num_rx 
	 << " period=" << e->_period << " pct=" << calc_pct(ol->tau, e->_period, ol->num_rx);
    }
    else
      sa << "age=-1 tau=-1 num_rx=-1 period=-1 pct=-1";

    sa << " -- rev ";
    if (pl) {
      unsigned num_rx = e->count_rx(pl);
      sa << "tau=" << e->_tau << " num_rx=" << num_rx << " period=" << pl->period 
	 << " pct=" << calc_pct(e->_tau, pl->period, num_rx);
    }
    else 
      sa << "tau=-1 num_rx=-1 period=-1 pct=-1";

    sa << '\n';
  }

  return sa.take_string();
}

unsigned int
LinkStat::calc_pct(unsigned tau, unsigned period, unsigned num_rx)
{
  if (period == 0)
    return 0;
  unsigned num_expected = tau / period;
  if (num_expected == 0)
    return 0;
  return 100 * num_rx / num_expected;
}


int
LinkStat::write_window(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  LinkStat *e = (LinkStat *) el;
  if (!cp_unsigned(arg, &e->_window))
    return errh->error("window must be >= 0");
  return 0;
}

int
LinkStat::write_period(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  LinkStat *e = (LinkStat *) el;
  if (!cp_unsigned(arg, &e->_period))
    return errh->error("period must be >= 0");

  return 0;
}

int
LinkStat::write_tau(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  LinkStat *e = (LinkStat *) el;
  if (!cp_unsigned(arg, &e->_tau))
    return errh->error("tau must be >= 0");
  return 0;
}


void
LinkStat::add_handlers()
{
  add_read_handler("bcast_stats", read_bcast_stats, 0);

  add_read_handler("window", read_window, 0);
  add_read_handler("tau", read_tau, 0);
  add_read_handler("period", read_period, 0);

  add_write_handler("window", write_window, 0);
  add_write_handler("tau", write_tau, 0);
  add_write_handler("period", write_period, 0);
}

EXPORT_ELEMENT(LinkStat)

#include <click/bighashmap.cc>
#include <click/vector.cc>
template class Vector<LinkStat::probe_t>;
template class BigHashMap<EtherAddress, LinkStat::probe_list_t>;
CLICK_ENDDECLS
