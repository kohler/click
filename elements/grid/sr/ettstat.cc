/*
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2003 Massachusetts Institute of Technology
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
#include "ettstat.hh"
#include "ettmetric.hh"
#include <click/packet_anno.hh>
CLICK_DECLS


#define MIN(x,y)      ((x)<(y) ? (x) : (y))
#define MAX(x,y)      ((x)>(y) ? (x) : (y));


ETTStat::ETTStat()
  : _window(100), 
    _tau(10000), 
    _period(1000), 
    _probe_size(1000),
    _seq(0), 
    _sent(0),
    _ett_metric(0),
    _arp_table(0),
    _timer_small(0),
    _timer_1(0),
    _timer_2(0),
    _timer_5(0),
    _timer_11(0)
{
  MOD_INC_USE_COUNT;
  add_input();
}

ETTStat::~ETTStat()
{
  MOD_DEC_USE_COUNT;
}

ETTStat *
ETTStat::clone() const
{
  return new ETTStat();
}

void
ETTStat::notify_noutputs(int n) 
{
  set_noutputs(n > 0 ? 1 : 0);  
}


int
ETTStat::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
			"IP", cpIPAddress, "IP address", &_ip,
			"WINDOW", cpUnsigned, "Broadcast loss rate window", &_window,
			"ETH", cpEtherAddress, "Source Ethernet address", &_eth,
			"PERIOD", cpUnsigned, "Probe broadcast period (msecs)", &_period,
			"TAU", cpUnsigned, "Loss-rate averaging period (msecs)", &_tau,
			"SIZE", cpUnsigned, "Probe size (bytes)", &_probe_size,
			"ETT", cpElement, "ETT Metric element", &_ett_metric,
			"ARP", cpElement, "ARPTable element", &_arp_table,
			0);
  if (res < 0)
    return res;
  
  unsigned min_sz = sizeof(click_ether) + sizeof(struct link_probe);
  if (_probe_size < min_sz)
    return errh->error("Specified packet size is less than the minimum probe size of %u",
		       min_sz);

  if (!_et) {
    return errh->error("Must specify ETHTYPE");
  }
  if (!_ip) {
    return errh->error("Invalid IPAddress specified\n");
  }

  if (!_eth) {
    return errh->error("Invalid EtherAddress specified\n");
  }

  if (_ett_metric && _ett_metric->cast("ETTMetric") == 0) {
    return errh->error("ETTMetric element is not a ETTMetric");
  }
  if (_arp_table && _arp_table->cast("ARPTable") == 0) {
    return errh->error("ARPTable element is not a ARPTable");
  }
  return res;
}


void
ETTStat::take_state(Element *e, ErrorHandler *)
{
  ETTStat *q = (ETTStat *)e->cast("ETTStat");
  if (!q) return;
  
  _neighbors = q->_neighbors;
  _bcast_stats = q->_bcast_stats;
  _rev_arp = q->_rev_arp;
  _seq = q->_seq;
  _sent = q->_sent;
}

void add_jitter(unsigned int max_jitter, struct timeval *t) {
  struct timeval jitter;
  unsigned j = (unsigned) (random() % (max_jitter + 1));
  unsigned int delta_us = 1000 * j;
  timerclear(&jitter);
  jitter.tv_usec += delta_us;
  jitter.tv_sec +=  jitter.tv_usec / 1000000;
  jitter.tv_usec = (jitter.tv_usec % 1000000);
  if (random() & 1) {
    timeradd(t, &jitter, t);
  } else {
    timersub(t, &jitter, t);
  }
  return;
}

void 
ETTStat::send_probe_hook(int rate) 
{
  unsigned max_jitter = _period / 10;
  unsigned int size = _probe_size;
  if (rate == 0) {
    size = sizeof(struct link_probe) + sizeof(click_ether);
  }
  send_probe(size, rate);

  struct timeval period;
  timerclear(&period);
  period.tv_usec += (_period * 1000);
  period.tv_sec += period.tv_usec / 1000000;
  period.tv_usec = (period.tv_usec % 1000000);

  switch (rate) {
  case 0:
    timeradd(&period, &_next_small, &_next_small);
    add_jitter(max_jitter, &_next_small);
    _timer_small->schedule_at(_next_small);
    break;
  case 1:
    timeradd(&period, &_next_1, &_next_1);
    add_jitter(max_jitter, &_next_1);
    _timer_1->schedule_at(_next_1);
    break;
  case 2:
    timeradd(&period, &_next_2, &_next_2);
    add_jitter(max_jitter, &_next_2);
    _timer_2->schedule_at(_next_2);
    break;
  case 5:
    timeradd(&period, &_next_5, &_next_5);
    add_jitter(max_jitter, &_next_5);
    _timer_5->schedule_at(_next_5);
    break;
  case 11:        
    timeradd(&period, &_next_11, &_next_11);
    add_jitter(max_jitter, &_next_11);
    _timer_11->schedule_at(_next_11);
    break;
  default:
    return;
  }
}

void
ETTStat::send_probe(unsigned int size, int rate) 
{

  unsigned min_packet_sz = sizeof(click_ether) + sizeof(struct link_probe);
  
  if (size < min_packet_sz) {
    click_chatter("%{element} cannot send packet size %d: min is %d\n",
		  this, 
		  size,
		  min_packet_sz);
    return;
  }

  WritablePacket *p = Packet::make(size + 2); // +2 for alignment
  if (p == 0) {
    click_chatter("ETTStat %s: cannot make packet!", id().cc());
    return;
  }
  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  struct timeval now;
  click_gettimeofday(&now);
  p->set_timestamp_anno(now);
  
  // fill in ethernet header 
  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _eth.data(), 6);

  // calculate number of entries

  unsigned max_entries = (size - min_packet_sz) / sizeof(struct link_entry);
  
  static bool size_warning = false;
  if (rate != 0 && !size_warning && max_entries < (unsigned) _neighbors.size()) {
    size_warning = true;
    click_chatter("ETTStat %s: WARNING, probe packet is too small; rotating entries", id().cc());
  }
  

  
  unsigned num_entries = max_entries < (unsigned) _neighbors.size() 
    ? max_entries 
    : _neighbors.size();
  
  if (rate == 0 && _sent <= (_tau / _period)+1) {
    _sent++;
  }
  

  link_probe *lp = (struct link_probe *) (p->data() + sizeof(click_ether));
  lp->ip = _ip.addr();
  lp->seq_no = _seq;
  lp->period = _period;
  lp->tau = _tau;
  lp->sent = _sent;
  lp->num_links = num_entries;
  lp->flags = 0;
  switch (rate) {
  case 0: 
    lp->flags |= PROBE_SMALL;
    break;
  case 1:
    lp->flags |= PROBE_1;
    break;
  case 2:
    lp->flags |= PROBE_2;
    break;
  case 5:
    lp->flags |= PROBE_5;
    break;
  case 11:
    lp->flags |= PROBE_11;
    break;
  default:
    click_chatter("weird rate %d\n", 
		  rate);
  }
  _seq++;
  
  if (rate != 0) {
    link_entry *entry = (struct link_entry *)(lp+1); 
    for (int i = 0; (unsigned) i < num_entries; i++) {
      _next_neighbor_to_ad = (_next_neighbor_to_ad + 1) % _neighbors.size();
      probe_list_t *probe = _bcast_stats.findp(_neighbors[_next_neighbor_to_ad]);
      if (!probe) {
	click_chatter("%{element}: lookup for %s (i=%d), %d failed in ad \n", 
		      this,
		      _neighbors[_next_neighbor_to_ad].s().cc(),
		      i,
		      _next_neighbor_to_ad);
      } else {
	entry->ip = probe->ip;
	entry->fwd_small = probe->fwd_small;
	entry->fwd_1 = probe->fwd_1;
	entry->fwd_2 = probe->fwd_2;
	entry->fwd_5 = probe->fwd_5;
	entry->fwd_11 = probe->fwd_11;
	
	entry->rev_small = probe->rev_rate(_start, 0);
	entry->rev_1 = probe->rev_rate(_start, 1);
	entry->rev_2 = probe->rev_rate(_start, 2);
	entry->rev_5 = probe->rev_rate(_start, 5);
	entry->rev_11 = probe->rev_rate(_start, 11);
      }
      entry = (entry+1);
    }
  }
  lp->psz = sizeof(link_probe) + lp->num_links*sizeof(link_entry);
  lp->cksum = 0;
  lp->cksum = click_in_cksum((unsigned char *) lp, lp->psz);
  

  SET_WIFI_FROM_CLICK(p);
  SET_WIFI_RATE_ANNO(p, rate);
  if (rate == 0) {
    SET_WIFI_RATE_ANNO(p, 1);
  }
  checked_output_push(0, p);
}

int
ETTStat::initialize(ErrorHandler *errh)
{
  if (noutputs() > 0) {
    if (!_eth) 
      return errh->error("Source Ethernet address must be specified to send probes");
    
    
    _timer_small = new Timer(static_send_small_hook, this);
    _timer_small->initialize(this);

    _timer_1 = new Timer(static_send_1_hook, this);
    _timer_1->initialize(this);

    _timer_2 = new Timer(static_send_2_hook, this);
    _timer_2->initialize(this);

    _timer_5 = new Timer(static_send_5_hook, this);
    _timer_5->initialize(this);

    _timer_11 = new Timer(static_send_11_hook, this);
    _timer_11->initialize(this);

    struct timeval now;
    click_gettimeofday(&now);
    
    _next_small = now;
    _next_1 = now;
    _next_2 = now;
    _next_5 = now;
    _next_11 = now;
    
    _next_small.tv_sec += 1;
    _next_1.tv_sec += 2;
    _next_2.tv_sec += 3;
    _next_5.tv_sec += 4;
    _next_11.tv_sec += 5;

    _timer_small->schedule_at(_next_small);
    _timer_1->schedule_at(_next_1);
    _timer_2->schedule_at(_next_2);
    _timer_5->schedule_at(_next_5);
    _timer_11->schedule_at(_next_11);
  }
  click_gettimeofday(&_start);
  return 0;
}



Packet *
ETTStat::simple_action(Packet *p)
{
  unsigned min_sz = sizeof(click_ether) + sizeof(link_probe);
  if (p->length() < min_sz) {
    click_chatter("ETTStat %s: packet is too small", id().cc());
    p->kill(); 
    return 0;
  }

  click_ether *eh = (click_ether *) p->data();

  if (ntohs(eh->ether_type) != _et) {
    click_chatter("ETTStat %s: got non-ETTStat packet type", id().cc());
    p->kill();
    return 0;
  }
  link_probe *lp = (link_probe *)(p->data() + sizeof(click_ether));


  if (click_in_cksum((unsigned char *) lp, lp->psz) != 0) {
    click_chatter("ETTStat %s: failed checksum", id().cc());
    p->kill();
    return 0;
  }


  if (p->length() < lp->psz + sizeof(click_ether)) {
    click_chatter("ETTStat %s: packet is smaller (%d) than it claims (%u)",
		  id().cc(), p->length(), lp->psz);
  }


  IPAddress ip = IPAddress(lp->ip);
  if (_arp_table) {
    _arp_table->insert(ip, EtherAddress(eh->ether_shost));
    _rev_arp.insert(EtherAddress(eh->ether_shost), ip);
  }

  struct timeval now;
  click_gettimeofday(&now);
  probe_t probe(now, lp->seq_no);
  int new_period = lp->period;
  probe_list_t *l = _bcast_stats.findp(ip);

  if (!l) {
    _bcast_stats.insert(ip, probe_list_t(ip, new_period, lp->tau));
    l = _bcast_stats.findp(ip);
    l->sent = 0;
    /* add into the neighbors vector */
    _neighbors.push_back(ip);
  } else if (l->period != new_period) {
    click_chatter("ETTStat %s: %s has changed its link probe period from %u to %u; clearing probe info\n",
		  id().cc(), ip.s().cc(), l->period, new_period);
    l->probes_small.clear();
    l->probes_1.clear();
    l->probes_2.clear();
    l->probes_5.clear();
    l->probes_11.clear();
  }

  if (lp->sent < (unsigned)l->sent) {
    click_chatter("ETTStat %s: %s has reset; clearing probe info\n",
		  id().cc(), ip.s().cc());
    l->probes_small.clear();
    l->probes_1.clear();
    l->probes_2.clear();
    l->probes_5.clear();
    l->probes_11.clear();
  }

  l->period = new_period;
  l->tau = lp->tau;
  l->sent = lp->sent;
  l->last_rx = now;

  if (lp->flags & PROBE_SMALL) {
    l->probes_small.push_back(probe);
  } else if (lp->flags & PROBE_1) {
      l->probes_1.push_back(probe);
  } else if (lp->flags & PROBE_2) {
      l->probes_2.push_back(probe);
  } else if (lp->flags & PROBE_5) {
      l->probes_5.push_back(probe);
  } else if (lp->flags & PROBE_11) {
      l->probes_11.push_back(probe);
  } else {
    click_chatter("Weird probe\n");
  }


  
  /* only keep stats for last _window *unique* sequence numbers */
  while ((unsigned) l->probes_small.size() > _window) 
    l->probes_small.pop_front();

  while ((unsigned) l->probes_1.size() > _window) 
    l->probes_1.pop_front();

  while ((unsigned) l->probes_2.size() > _window) 
    l->probes_2.pop_front();

  while ((unsigned) l->probes_5.size() > _window) 
    l->probes_5.pop_front();

  while ((unsigned) l->probes_11.size() > _window) 
    l->probes_11.pop_front();



  // look in received packet for info about our outgoing link
  unsigned int max_entries = (p->length() - sizeof(*eh) - sizeof(link_probe)) / sizeof(link_entry);
  unsigned int num_entries = lp->num_links;
  if (num_entries > max_entries) {
    click_chatter("ETTStat %s: WARNING, probe packet from %s contains fewer link entries (at most %u) than claimed (%u)", 
		  id().cc(), IPAddress(lp->ip).s().cc(), max_entries, num_entries);
    num_entries = max_entries;
  }


  const unsigned char *d = p->data() + sizeof(click_ether) + sizeof(link_probe);
  for (unsigned i = 0; i < num_entries; i++, d += sizeof(link_entry)) {
    link_entry *le = (struct link_entry *) d;
    if (IPAddress(le->ip) == _ip) {
      l->fwd_small = le->rev_small;
      l->fwd_1 = le->rev_1;
      l->fwd_2 = le->rev_2;
      l->fwd_5 = le->rev_5;
      l->fwd_11 = le->rev_11;
    } else {
      if (_ett_metric) {
	_ett_metric->update_link(ip, le->ip, 
				 le->fwd_small, le->rev_small,
				 le->fwd_1, le->rev_1,
				 le->fwd_2, le->rev_2,
				 le->fwd_5, le->rev_5,
				 le->fwd_11, le->rev_11
				 );
      }
    }
  }

  /* 
   * always update the metric from us to them, even if we didn't hear 
   * the fwd metric
   */
  if (_ett_metric) {
    _ett_metric->update_link(_ip, ip, 
			     l->fwd_small, l->rev_rate(_start, 0),
			     l->fwd_1, l->rev_rate(_start, 1),
			     l->fwd_2, l->rev_rate(_start, 2),
			     l->fwd_5, l->rev_rate(_start, 5),
			     l->fwd_11, l->rev_rate(_start, 11)
			     );
  }
  
  p->kill();
  return 0;
}
  


int 
ETTStat::get_etx(int fwd, int rev) {

  fwd = MIN(fwd, 100);
  rev = MIN(rev, 100);
  
  if (fwd > 0 && rev > 0) {
    if (fwd >= 80 && rev >= 80) {
      fwd += ((fwd/10)-7)*20;
      rev += ((rev/10)-7)*20;
    }  
    int val = (100 * 100 * 100) / (fwd * rev);
    return val;
  } 
  
  return 7777;

}
void
ETTStat::update_links(IPAddress ip) {
  probe_list_t *l = _bcast_stats.findp(ip);
  if (_ett_metric) {
    if (l) {
      _ett_metric->update_link(_ip, ip,
			       l->fwd_small, l->rev_rate(_start, 0),
			       l->fwd_1, l->rev_rate(_start, 1),
			       l->fwd_2, l->rev_rate(_start, 2),
			       l->fwd_5, l->rev_rate(_start, 5),
			       l->fwd_11, l->rev_rate(_start, 11)
			       );
    } else {
      _ett_metric->update_link(_ip, ip,
			       0,0,
			       0,0,
			       0,0,
			       0,0,
			       0,0
			       );
    }			       
  }
}


String
ETTStat::read_window(Element *xf, void *)
{
  ETTStat *f = (ETTStat *) xf;
  return String(f->_window) + "\n";
}

String
ETTStat::read_period(Element *xf, void *)
{
  ETTStat *f = (ETTStat *) xf;
  return String(f->_period) + "\n";
}

String
ETTStat::read_tau(Element *xf, void *)
{
  ETTStat *f = (ETTStat *) xf;
  return String(f->_tau) + "\n";
}

String
ETTStat::read_bcast_stats(Element *xf, void *)
{
  ETTStat *e = (ETTStat *) xf;

  typedef HashMap<IPAddress, bool> IPMap;
  IPMap ip_addrs;
  
  for (ProbeMap::const_iterator i = e->_bcast_stats.begin(); i; i++) 
    ip_addrs.insert(i.key(), true);

  struct timeval now;
  click_gettimeofday(&now);

  StringAccum sa;
  for (IPMap::const_iterator i = ip_addrs.begin(); i; i++) {
    IPAddress ip  = i.key();
    probe_list_t *pl = e->_bcast_stats.findp(ip);
    
    sa << ip;
    if (e->_arp_table) {
      EtherAddress eth_dest = e->_arp_table->lookup(ip);
      if (eth_dest) {
	sa << " " << eth_dest.s().cc();
      } else {
	sa << " ?";
      }
    } else {
      sa << " ?";
    }
    sa << " period " << pl->period;
    sa << " tau " << pl->tau;
    sa << " sent " << pl->sent;
    sa << " fwd_small " << (int)pl->fwd_small;
    sa << " rev_small " << pl->rev_rate(e->_start, 0);
    sa << " fwd_1 " << (int)pl->fwd_1;
    sa << " rev_1 " << pl->rev_rate(e->_start, 1);
    sa << " fwd_2 " << (int)pl->fwd_2;
    sa << " rev_2 " << pl->rev_rate(e->_start, 2);
    sa << " fwd_5 " << (int)pl->fwd_5;
    sa << " rev_5 " << pl->rev_rate(e->_start, 5);
    sa << " fwd_11 " << (int)pl->fwd_11;
    sa << " rev_11 " << pl->rev_rate(e->_start, 11);
    sa << " etx " << e->get_etx(pl->fwd_1, pl->rev_rate(e->_start, 0));
    sa << " last_rx " << now - pl->last_rx;
    sa << "\n";
    
  }

  return sa.take_string();
}


int
ETTStat::write_window(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  ETTStat *e = (ETTStat *) el;
  if (!cp_unsigned(arg, &e->_window))
    return errh->error("window must be >= 0");
  return 0;
}

int
ETTStat::write_period(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  ETTStat *e = (ETTStat *) el;
  if (!cp_unsigned(arg, &e->_period))
    return errh->error("period must be >= 0");

  return 0;
}

int
ETTStat::write_tau(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  ETTStat *e = (ETTStat *) el;
  if (!cp_unsigned(arg, &e->_tau))
    return errh->error("tau must be >= 0");
  return 0;
}


void
ETTStat::add_handlers()
{
  add_read_handler("bcast_stats", read_bcast_stats, 0);

  add_read_handler("window", read_window, 0);
  add_read_handler("tau", read_tau, 0);
  add_read_handler("period", read_period, 0);

  add_write_handler("window", write_window, 0);
  add_write_handler("tau", write_tau, 0);
  add_write_handler("period", write_period, 0);
}
IPAddress 
ETTStat::reverse_arp(EtherAddress eth)
{
  IPAddress *ip = _rev_arp.findp(eth);
  return (ip) ? IPAddress(ip->addr()) : IPAddress();
}


EXPORT_ELEMENT(ETTStat)

#include <click/bighashmap.cc>
#include <click/dequeue.cc>
#include <click/vector.cc>
template class DEQueue<ETTStat::probe_t>;
template class HashMap<IPAddress, ETTStat::probe_list_t>;
CLICK_ENDDECLS
