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
#include "linkstat.hh"
#include <click/glue.hh>
#include <click/timer.hh>
#include <sys/time.h>
#include "grid.hh"
#include "timeutils.hh"
CLICK_DECLS

LinkStat::LinkStat()
  : _ai(0), _period(1000), _send_timer(0)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
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
  int new_n = 1;
  if (n > 2) new_n = 2;
  if (n < 1) new_n = 1;
  set_noutputs(new_n);  
}


int
LinkStat::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpElement, "AiroInfo element", &_ai,
			cpUnsigned, "Broadcast loss rate window (msecs)", &_window,
			cpOptional,
			cpEtherAddress, "Source Ethernet address", &_eth,
			cpIPAddress, "Source IP address", &_ip,
			cpUnsigned, "Probe broadcast period (msecs)", &_period,
			0);
  if (res < 0)
    return res;
  
  return res;
}

void
LinkStat::send_hook() 
{
  unsigned int psz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_link_probe);
  WritablePacket *p = Packet::make(psz + 2); // +2 for alignment
  if (p == 0) {
    click_chatter("LinkStat %s: cannot make packet!", id().cc());
    return;
  }
  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  struct timeval tv;
  int res = gettimeofday(&tv, 0);
  if (res == 0) 
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
  gh->total_len = htons(psz - sizeof(click_ether));
  gh->type = grid_hdr::GRID_LINK_PROBE;
  gh->ip = gh->tx_ip = _ip;
  
  grid_link_probe *glp = (grid_link_probe *) (gh + 1);
  glp->seq_no = htonl(_seq);
  _seq++;
  glp->period = htonl(_period);
  glp->window = htonl(_window);
  glp->num_links = htonl(0);
  
  checked_output_push(1, p);
}

int
LinkStat::initialize(ErrorHandler *errh)
{
  if (noutputs() > 1) {
    if (!_eth || !_ip) 
      return errh->error("Source IP and Ethernet address must be specified to send probes");
    _send_timer = new Timer(static_send_hook, this);
    _send_timer->initialize(this);
    _send_timer->schedule_after_ms(_period);
  }
  return 0;
}



Packet *
LinkStat::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  EtherAddress ea(eh->ether_shost);

  stat_t s;
  gettimeofday(&s.when, 0);

  bool res1 = _ai->get_signal_info(ea, s.sig, s.qual);
  int t1, t2;
  bool res2 = _ai->get_noise(s.noise, t1, t2); 
  if (res1 || res2)
    _stats.insert(ea, s);

  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("LinkStat %s: got non-Grid packet type", id().cc());
    return p;
  }

  grid_hdr *gh = (grid_hdr *) (eh + 1);
  if (gh->type != grid_hdr::GRID_LINK_PROBE)
    return p;

  grid_link_probe *lp = (grid_link_probe *) (gh + 1);
  add_bcast_stat(ea, gh->ip, lp);

  return p;
}

void
LinkStat::remove_all_stats(const EtherAddress &e)
{
  _stats.remove(e);
  /* don't want to remove bcast stats: window may be > expire timer */
  // _bcast_stats.remove(e);

}

bool
LinkStat::get_bcast_stats(const EtherAddress &e, struct timeval &last, unsigned int &window,
			  unsigned int &num_rx, unsigned int &num_expected)
{
  probe_list_t *l = _bcast_stats.findp(e);
  if (!l || l->probes.size() == 0)
    return false;

  unsigned int first_seq, last_seq;
  first_seq = l->probes.at(0).seq_no;
  last_seq = l->probes.back().seq_no;

  num_expected = 1 + (last_seq - first_seq);
  num_rx = l->probes.size();
  last = l->probes.back().when;
  
  window = _window;

  return true;
}

void
LinkStat::add_bcast_stat(const EtherAddress &e, const IPAddress &ip, const grid_link_probe *lp)
{
  struct timeval now;
  gettimeofday(&now, 0);
  probe_t probe(now, ntohl(lp->seq_no));
  
  unsigned int new_period = ntohl(lp->period);
  
  probe_list_t *l = _bcast_stats.findp(e);
  if (!l) {
    probe_list_t l2(ip, new_period, ntohl(lp->window));
    _bcast_stats.insert(e, l2);
    l = _bcast_stats.findp(e);
  }
  else if (l->period != new_period) {
    click_chatter("LinkStat %s: node %s has changed its link probe beriod from %u to %u; clearing probe info\n",
		  id().cc(), ip.s().cc(), l->period, new_period);
    l->probes.clear();
    return;
  }

  
  l->probes.push_back(probe);
  

  /* only keep stats for last _window msecs */
  struct timeval delta;
  delta.tv_sec = _window / 1000;
  delta.tv_usec = (_window % 1000) * 1000;
  struct timeval last = now - delta;
      
  Vector<probe_t> new_vec;
  for (int i = 0; i < l->probes.size(); i++) 
    if (l->probes.at(i).when >= last)
      new_vec.push_back(l->probes.at(i));
  
  l->probes = new_vec;  
}

String
LinkStat::read_window(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;
  return String(f->_window) + "\n";
}



String
LinkStat::read_stats(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;

  String s;

  for (BigHashMap<EtherAddress, LinkStat::stat_t>::iterator i = f->_stats.begin(); i; i++) {
    char timebuf[80];
    snprintf(timebuf, 80, " %lu.%06lu", i.value().when.tv_sec, i.value().when.tv_usec);
    s += i.key().s() + String(timebuf) + " sig: " + String(i.value().sig) + ", qual: " + String(i.value().qual) +
      ", noise: " + String(i.value().noise) + "\n";
  }
  return s;
}

String
LinkStat::read_bcast_stats(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;

  Vector<EtherAddress> e_vec;
  int num = 0;

  String s;
  for (BigHashMap<EtherAddress, probe_list_t>::iterator i = f->_bcast_stats.begin(); i; i++) {
    e_vec.push_back(i.key());
    num++;
  }

  for (int i = 0; i < num; i++) {
    struct timeval when;
    unsigned int window, num_rx, num_expected;    
    bool res = f->get_bcast_stats(e_vec[i], when, window, num_rx, num_expected);
    if (!res || window != f->_window) 
      return "Error: inconsistent data structures\n";

    char timebuf[80];
    snprintf(timebuf, 80, "%lu.%06lu", when.tv_sec, when.tv_usec);
    s += " period=" + String(f->_bcast_stats[e_vec[i]].period) + " window=" + String(f->_bcast_stats[e_vec[i]].window);
    s += e_vec[i].s() + " last=" + String(timebuf) + " num_rx=" + String(num_rx) + " num_expected=" + String(num_expected) + "\n";
  }
  return s;
}


int
LinkStat::write_window(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  LinkStat *e = (LinkStat *) el;
  int window = atoi(((String) arg).cc());
  if (window < 0)
    return errh->error("window must be >= 0");
  e->_window = window;

  /* clear all stats to avoid confusing data */
  e->_bcast_stats.clear();

  return 0;
}


void
LinkStat::add_handlers()
{
  add_read_handler("stats", read_stats, 0);
  add_read_handler("bcast_stats", read_bcast_stats, 0);
  add_read_handler("window", read_window, 0);
  add_write_handler("window", write_window, 0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LinkStat)

#include <click/bighashmap.cc>
#include <click/vector.cc>
template class BigHashMap<EtherAddress, LinkStat::stat_t>;
template class Vector<LinkStat::probe_t>;
template class BigHashMap<EtherAddress, LinkStat::probe_list_t>;
CLICK_ENDDECLS
