/*
 * John Bicket
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
#include "ettstat.hh"
#include "ettmetric.hh"
#include "txcountmetric.hh"
#include <click/packet_anno.hh>
#include <elements/wifi/availablerates.hh>
CLICK_DECLS
// packet data should be 4 byte aligned                                         
#define ASSERT_ALIGNED(p) assert(((unsigned int)(p) % 4) == 0)

#define min(x,y)      ((x)<(y) ? (x) : (y))
#define max(x,y)      ((x)>(y) ? (x) : (y));


ETTStat::ETTStat()
  : _window(100), 
    _tau(10000), 
    _period(1000), 
    _sent(0),
    _ett_metric(0),
    _etx_metric(0),
    _arp_table(0),
    _next_neighbor_to_ad(0),
    _timer(0),
    _ads_rs_index(0),
    _rtable(0)
{
  MOD_INC_USE_COUNT;
  add_input();
}

ETTStat::~ETTStat()
{
  MOD_DEC_USE_COUNT;
}

void
ETTStat::notify_noutputs(int n) 
{
  set_noutputs(n > 0 ? 1 : 0);  
}


//1. configure
//2. initialize
//3. take_state
int
ETTStat::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String probes;
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
			"IP", cpIPAddress, "IP address", &_ip,
			"WINDOW", cpUnsigned, "Broadcast loss rate window", &_window,
			"ETH", cpEtherAddress, "Source Ethernet address", &_eth,
			"PERIOD", cpUnsigned, "Probe broadcast period (msecs)", &_period,
			"TAU", cpUnsigned, "Loss-rate averaging period (msecs)", &_tau,
			"ETT", cpElement, "ETT Metric element", &_ett_metric,
			"ETX", cpElement, "ETX Metric element", &_etx_metric,
			"ARP", cpElement, "ARPTable element", &_arp_table,
			"PROBES", cpString, "PROBES", &probes,
			"RT", cpElement, "AvailabeRates", &_rtable,
			cpEnd);


  Vector<String> a;
  cp_spacevec(probes, a);

  for (int x = 0; x < a.size() - 1; x += 2) {
    int rate;
    int size;
    if (!cp_integer(a[x], &rate)) {
      return errh->error("invalid PROBES rate value\n");
    }
    if (!cp_integer(a[x + 1], &size)) {
      return errh->error("invalid PROBES size value\n");
    }
    _ads_rs.push_back(RateSize(rate, size));
  }

  if (!_ads_rs.size()) {
    return errh->error("no PROBES provided\n");
  }
  
  if (res < 0)
    return res;

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
  if (_etx_metric && _etx_metric->cast("TXCountMetric") == 0) {
    return errh->error("ETXMetric element is not a ETXMetric");
  }
  if (_rtable && _rtable->cast("AvailableRates") == 0) {
    return errh->error("RT element is not a AvailableRates");
  }
  if (_arp_table && _arp_table->cast("ARPTable") == 0) {
    return errh->error("ARPTable element is not a ARPTable");
  }

  return res;
}


void
ETTStat::take_state(Element *e, ErrorHandler *errh)
{
  /* 
   * take_state gets called after 
   * --configure
   * --initialize
   * so we may need to unschedule probe timers
   * and sync them up so the rates don't get 
   * screwed up.
  */
  ETTStat *q = (ETTStat *)e->cast("ETTStat");
  if (!q) {
    errh->error("Couldn't cast old ETTStat");
    return;
  }
  
  _neighbors = q->_neighbors;
  _bcast_stats = q->_bcast_stats;
  _rev_arp = q->_rev_arp;
  _sent = q->_sent;
  _start = q->_start;

  struct timeval now;
  click_gettimeofday(&now);

  if (timercmp(&now, &q->_next, <)) {
    _timer->unschedule();
    _timer->schedule_at(q->_next);
    _next = q->_next;
  }

  
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
ETTStat::update_link(IPAddress from, IPAddress to, Vector<RateSize> rs, Vector<int> fwd, Vector<int> rev, uint32_t seq)
{
  if (_ett_metric) {
    _ett_metric->update_link(from, to, rs, fwd, rev, seq);
  }

  if (_etx_metric) {
    _etx_metric->update_link(from, to, rs, fwd, rev, seq);
  }
  
}
  





void 
ETTStat::send_probe_hook() 
{

  unsigned max_jitter = _period / 10;

  send_probe();
  

  struct timeval period;
  timerclear(&period);
  int p = _period / _ads_rs.size();
  period.tv_usec += (p * 1000);
  period.tv_sec += period.tv_usec / 1000000;
  period.tv_usec = (period.tv_usec % 1000000);

  timeradd(&period, &_next, &_next);
  add_jitter(max_jitter, &_next);
  _timer->schedule_at(_next);
}

void
ETTStat::send_probe() 
{

  if (!_ads_rs.size()) {
    click_chatter("%{element} :: %s no probes to send at\n",
		  this,
		  __func__);
    return;
  }

  int size = _ads_rs[_ads_rs_index]._size;
  int rate = _ads_rs[_ads_rs_index]._rate;

  _ads_rs_index = (_ads_rs_index + 1) % _ads_rs.size();

  if (_ads_rs_index == 0) {
    _sent++;
  }
  unsigned min_packet_sz = sizeof(click_ether) + sizeof(struct link_probe);

  if ((unsigned) size < min_packet_sz) {
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

  link_probe *lp = (struct link_probe *) (p->data() + sizeof(click_ether));
  lp->_version = _ett_version;
  lp->ip = _ip.addr();
  lp->seq = now.tv_sec;
  lp->period = _period;
  lp->tau = _tau;
  lp->sent = _sent;
  lp->flags = 0;
  lp->rate = rate;
  lp->size = size;
  lp->num_probes = _ads_rs.size();
  
  uint8_t *ptr =  (uint8_t *) (lp + 1);
  uint8_t *end  = (uint8_t *) p->data() + p->length();

  
  Vector<int> rates;
  if (_rtable) {
    rates = _rtable->lookup(_eth);
  }
  if (rates.size() && 1 + ptr + rates.size() < end) {
    ptr[0] = rates.size();
    ptr++;
    int x = 0;
    while (ptr < end && x < rates.size()) {
	ptr[x] = rates[x];
	x++;
    }
    ptr += rates.size();
    lp->flags |= PROBE_AVAILABLE_RATES;
  }

  int num_entries = 0;
  while (ptr < end && num_entries < _neighbors.size()) {
    _next_neighbor_to_ad = (_next_neighbor_to_ad + 1) % _neighbors.size();
    if (_next_neighbor_to_ad >= _neighbors.size()) {
      break;
    }
    probe_list_t *probe = _bcast_stats.findp(_neighbors[_next_neighbor_to_ad]);
    if (!probe) {
      click_chatter("%{element}: lookup for %s, %d failed in ad \n", 
		    this,
		    _neighbors[_next_neighbor_to_ad].s().cc(),
		    _next_neighbor_to_ad);
    } else {

      int size = probe->probe_types.size()*sizeof(link_info) + sizeof(link_entry);
      if (ptr + size > end) {
	break;
      }
      num_entries++;
      link_entry *entry = (struct link_entry *)(ptr); 
      entry->ip = probe->ip;
      entry->seq = probe->seq;	
      if ((uint32_t) probe->ip > (uint32_t) _ip) {
	entry->seq = lp->seq;
      }
      entry->num_rates = probe->probe_types.size();
      ptr += sizeof(link_entry);

      Vector<RateSize> rates;
      Vector<int> fwd;
      Vector<int> rev;

      for (int x = 0; x < probe->probe_types.size(); x++) {
	RateSize rs = probe->probe_types[x];
	link_info *lnfo = (struct link_info *) (ptr + x*sizeof(link_info));
	lnfo->size = rs._size;
	lnfo->rate = rs._rate;
	lnfo->fwd = probe->_fwd_rates[x];
	lnfo->rev = probe->rev_rate(_start, rs._rate, rs._size);
	
	rates.push_back(rs);
	fwd.push_back(lnfo->fwd);
	rev.push_back(lnfo->rev);
      }
      update_link(_ip, entry->ip, rates, fwd, rev, entry->seq);

      ptr += probe->probe_types.size()*sizeof(link_info);
    }

  }
  
  lp->num_links = num_entries;
  lp->psz = sizeof(link_probe) + lp->num_links*sizeof(link_entry);
  lp->cksum = 0;
  lp->cksum = click_in_cksum((unsigned char *) lp, lp->psz);
  

  SET_WIFI_FROM_CLICK(p);
  SET_WIFI_RATE_ANNO(p, rate);
  if (rate == 0) {
    SET_WIFI_RATE_ANNO(p, 2);
  }
  checked_output_push(0, p);
}



int
ETTStat::initialize(ErrorHandler *errh)
{
  if (noutputs() > 0) {
    if (!_eth) 
      return errh->error("Source Ethernet address must be specified to send probes");
    
    _timer = new Timer(static_send_hook, this);
    _timer->initialize(this);

    struct timeval now;
    click_gettimeofday(&now);
    
    _next = now;

    unsigned max_jitter = _period / 10;
    add_jitter(max_jitter, &_next);

    _timer->schedule_at(_next);
  }

  reset();
  return 0;
}



Packet *
ETTStat::simple_action(Packet *p)
{

  
  struct timeval now;
  click_gettimeofday(&now);

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
  if (lp->_version != _ett_version) {
    static bool version_warning = false;
    _bad_table.insert(EtherAddress(eh->ether_shost), lp->_version);
    if (!version_warning) {
      version_warning = true; 
      click_chatter ("%{element}: unknown sr version %x from %s", 
		     this,
		     lp->_version,
		     EtherAddress(eh->ether_shost).s().cc());
    }

    
    p->kill();
    return 0;
  }


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

  if (ip == _ip) {
    p->kill();
    return 0;
  }
  if (_arp_table) {
    _arp_table->insert(ip, EtherAddress(eh->ether_shost));
    _rev_arp.insert(EtherAddress(eh->ether_shost), ip);
  }

  uint8_t rate = WIFI_RATE_ANNO(p);
  probe_t probe(now, lp->seq, lp->rate, lp->size);
  int new_period = lp->period;
  probe_list_t *l = _bcast_stats.findp(ip);
  int x = 0;
  if (!l) {
    _bcast_stats.insert(ip, probe_list_t(ip, new_period, lp->tau));
    l = _bcast_stats.findp(ip);
    l->sent = 0;
    /* add into the neighbors vector */
    _neighbors.push_back(ip);
  } else if (l->period != new_period) {
    click_chatter("ETTStat %s: %s has changed its link probe period from %u to %u; clearing probe info",
		  id().cc(), ip.s().cc(), l->period, new_period);
    l->probes.clear();
  }
  
  RateSize rs = RateSize(rate, lp->size);
  l->period = new_period;
  l->tau = lp->tau;
  l->sent = lp->sent;
  l->last_rx = now;
  
  l->probes.push_back(probe);
  l->seq = probe._seq;
  /* only keep stats for last _window *unique* sequence numbers */
  while ((unsigned) l->probes.size() > _window) 
    l->probes.pop_front();

  
  for (x = 0; x < l->probe_types.size(); x++) {
    if (rs == l->probe_types[x]) {
      break;
    }
  }
  
  if (x == l->probe_types.size()) {
    l->probe_types.push_back(rs);
    l->_fwd_rates.push_back(0);
  }

  if (lp->sent < (unsigned)l->sent) {
    click_chatter("ETTStat %s: %s has reset; clearing probe info",
		  id().cc(), ip.s().cc());
    l->probes.clear();
  }

  




  uint8_t *ptr =  (uint8_t *) (lp + 1);

  uint8_t *end  = (uint8_t *) p->data() + p->length();


  if (lp->flags &= PROBE_AVAILABLE_RATES) {
    int num_rates = ptr[0];
    Vector<int> rates;
    ptr++;
    int x = 0;
    while (ptr < end && x < num_rates) {
      int rate = ptr[x];
      rates.push_back(rate);
      x++;
    }
    ptr += num_rates;
    if(_rtable) {
      _rtable->insert(EtherAddress(eh->ether_shost), rates);
    }
  }
  int link_number = 0;
  while (ptr < end && (unsigned) link_number < lp->num_links) {
    link_number++;
    link_entry *entry = (struct link_entry *)(ptr); 
    IPAddress neighbor = entry->ip;
    int num_rates = entry->num_rates;
    if (0) {
      click_chatter("%{element} on link number %d / %d: neighbor %s, num_rates %d\n",
		    this,
		    link_number,
		    lp->num_links,
		    neighbor.s().cc(),
		    num_rates);
    }

    ptr += sizeof(struct link_entry);
    Vector<RateSize> rates;
    Vector<int> fwd;
    Vector<int> rev;
    for (int x = 0; x < num_rates; x++) {
      struct link_info *nfo = (struct link_info *) (ptr + x * (sizeof(struct link_info)));

      if (0) {
	click_chatter("%{element} on %s neighbor %s: size %d rate %d fwd %d rev %d\n",
		      this,
		      ip.s().cc(),
		      neighbor.s().cc(),
		      nfo->size,
		      nfo->rate,
		      nfo->fwd,
		      nfo->rev);
      }
      
      RateSize rs = RateSize(nfo->rate, nfo->size);
      /* update other link stuff */
      rates.push_back(rs);
      fwd.push_back(nfo->fwd);
      if (neighbor == _ip) {
	rev.push_back(l->rev_rate(_start, rates[x]._rate, rates[x]._size));
      } else {
	rev.push_back(nfo->rev);
      }

      if (neighbor == _ip) {
	/* set the fwd rate */
	for (int x = 0; x < l->probe_types.size(); x++) {
	  if (rs == l->probe_types[x]) {
	    l->_fwd_rates[x] = nfo->rev;
	    
	    break;
	  }
	}
      }
    }
    int seq = entry->seq;
    if (neighbor == ip && 
	((uint32_t) neighbor > (uint32_t) _ip)) {
      seq = now.tv_sec;
    }
    update_link(ip, neighbor, rates, fwd, rev, seq);
    ptr += num_rates * sizeof(struct link_info);
    
  }

  p->kill();
  return 0;
}
  

String
ETTStat::read_bcast_stats()
{
  typedef HashMap<IPAddress, bool> IPMap;
  IPMap ip_addrs;
  
  for (ProbeMap::const_iterator i = _bcast_stats.begin(); i; i++) 
    ip_addrs.insert(i.key(), true);

  struct timeval now;
  click_gettimeofday(&now);

  StringAccum sa;
  for (IPMap::const_iterator i = ip_addrs.begin(); i; i++) {
    IPAddress ip  = i.key();
    probe_list_t *pl = _bcast_stats.findp(ip);
    //sa << _ip << " " << _eth << " ";
    sa << ip;
    if (_arp_table) {
      EtherAddress eth_dest = _arp_table->lookup(ip);
      if (eth_dest) {
	sa << " " << eth_dest.s().cc();
      } else {
	sa << " ?";
      }
    } else {
      sa << " ?";
    }
    for (int x = 0; x < pl->probe_types.size(); x++) {
      sa << " [ " << pl->probe_types[x]._rate << " ";
      sa << pl->probe_types[x]._size << " ";
      int rev_rate = pl->rev_rate(_start, pl->probe_types[x]._rate, 
				  pl->probe_types[x]._size);
      sa << pl->_fwd_rates[x] << " ";
      sa << rev_rate << " ]";
    }
    sa << " seq " << pl->seq;
    sa << " period " << pl->period;
    sa << " tau " << pl->tau;
    sa << " sent " << pl->sent;
    sa << " last_rx " << now - pl->last_rx;
    sa << "\n";
    
  }

  return sa.take_string();
}


String 
ETTStat::bad_nodes() {

  StringAccum sa;
  for (BadTable::const_iterator i = _bad_table.begin(); i; i++) {
    uint8_t version = i.value();
    EtherAddress dst = i.key();
    sa << this << " eth " << dst.s().cc() << " version " << (int) version << "\n";
  }

  return sa.take_string();
}


void
ETTStat::reset()
{
  _neighbors.clear();
  _bcast_stats.clear();
  _rev_arp.clear();
  _seq = 0;
  _sent = 0;
  click_gettimeofday(&_start);
}
enum {
  H_RESET,
  H_BCAST_STATS,
  H_BAD_VERSION,
};

static String 
ETTStat_read_param(Element *e, void *thunk)
{
  ETTStat *td = (ETTStat *)e;
    switch ((uintptr_t) thunk) {
    case H_BCAST_STATS: return td->read_bcast_stats();
    case H_BAD_VERSION: return td->bad_nodes();
    default:
      return String() + "\n";
    }
}
static int 
ETTStat_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *)
{
  ETTStat *f = (ETTStat *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_RESET: {    //reset
    f->reset();
    break;
  }
  }
  return 0;
}

void
ETTStat::add_handlers()
{
  add_read_handler("bcast_stats", ETTStat_read_param, (void *) H_BCAST_STATS);
  add_read_handler("bad_version", ETTStat_read_param, (void *) H_BAD_VERSION);
  add_write_handler("reset", ETTStat_write_param, (void *) H_RESET);

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
template class HashMap<EtherAddress, uint8_t>;
CLICK_ENDDECLS
