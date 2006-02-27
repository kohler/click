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
#include <clicknet/wifi.h>
#include <elements/wifi/availablerates.hh>
CLICK_DECLS
// packet data should be 4 byte aligned                                         
#define ASSERT_ALIGNED(p) assert(((uintptr_t)(p) % 4) == 0)

#define min(x,y)      ((x)<(y) ? (x) : (y))
#define max(x,y)      ((x)>(y) ? (x) : (y))

enum {
  H_RESET,
  H_BCAST_STATS,
  H_BAD_VERSION,
  H_IP,
  H_TAU,
  H_PERIOD,
  H_PROBES,
};

static String 
ETTStat_read_param(Element *e, void *thunk)
{
  ETTStat *td = (ETTStat *)e;
    switch ((uintptr_t) thunk) {
    case H_BCAST_STATS: return td->read_bcast_stats();
    case H_BAD_VERSION: return td->bad_nodes();
    case H_IP: return td->_ip.s() + "\n";
    case H_TAU: return String(td->_tau) + "\n";
    case H_PERIOD: return String(td->_period) + "\n";
    case H_PROBES: {
      StringAccum sa;
      for(int x = 0; x < td->_ads_rs.size(); x++) {
	sa << td->_ads_rs[x]._rate << " " << td->_ads_rs[x]._size << " ";
      }
      return sa.take_string() + "\n";
    }
    default:
      return String() + "\n";
    }
}
static int 
ETTStat_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  ETTStat *f = (ETTStat *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_RESET: {    //reset
    f->reset();
    break;
  }
  case H_TAU: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("tau parameter must be unsigned");
    f->_tau = m;
    f->reset();
  }

  case H_PERIOD: {
    unsigned m;
    if (!cp_unsigned(s, &m)) 
      return errh->error("period parameter must be unsigned");
    f->_period = m;
    f->reset();
  }
  case H_PROBES: {
    Vector<RateSize> ads_rs;
    Vector<String> a;
    cp_spacevec(s, a);
    if (a.size() % 2 != 0) {
      return errh->error("must provide even number of numbers\n");
    }
  for (int x = 0; x < a.size() - 1; x += 2) {
    int rate;
    int size;
    if (!cp_integer(a[x], &rate)) {
      return errh->error("invalid PROBES rate value\n");
    }
    if (!cp_integer(a[x + 1], &size)) {
      return errh->error("invalid PROBES size value\n");
    }
    ads_rs.push_back(RateSize(rate, size));
  }
  if (!ads_rs.size()) {
    return errh->error("no PROBES provided\n");
  }
  f->_ads_rs = ads_rs;
  }
    
    
  }
  return 0;
}


ETTStat::ETTStat()
  : _tau(10000), 
    _period(1000), 
    _sent(0),
    _ett_metric(0),
    _etx_metric(0),
    _arp_table(0),
    _next_neighbor_to_ad(0),
    _timer(this),
    _ads_rs_index(0),
    _rtable(0)
{
}

ETTStat::~ETTStat()
{
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
			"ETH", cpEtherAddress, "Source Ethernet address", &_eth,
			"PERIOD", cpUnsigned, "Probe broadcast period (msecs)", &_period,
			"TAU", cpUnsigned, "Loss-rate averaging period (msecs)", &_tau,
			"ETT", cpElement, "ETT Metric element", &_ett_metric,
			"ETX", cpElement, "ETX Metric element", &_etx_metric,
			"ARP", cpElement, "ARPTable element", &_arp_table,
			"PROBES", cpString, "PROBES", &probes,
			"RT", cpElement, "AvailabeRates", &_rtable,
			cpEnd);


  if (res < 0)
    return res;

  if ((res = ETTStat_write_param(probes, this, (void *) H_PROBES, errh)) < 0) {
  return res;
  }

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

void add_jitter(unsigned int max_jitter, Timestamp *t) {
  unsigned j = (unsigned) (random() % (max_jitter + 1));
  if (random() & 1) {
      *t += Timestamp::make_msec(j);
  } else {
      *t -= Timestamp::make_msec(j);
  }
  return;
}

void
ETTStat::run_timer(Timer *)
{
	int p = _period / _ads_rs.size();
	unsigned max_jitter = p / 10;
	
	send_probe();
	
	_next += Timestamp::make_msec(p);
	add_jitter(max_jitter, &_next);
	_timer.schedule_at(_next);
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

  if (Timestamp::now() < q->_next) {
    _timer.unschedule();
    _timer.schedule_at(q->_next);
    _next = q->_next;
  }

  
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

  _sent++;
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
	  click_chatter("%{element}: cannot make packet!", this);
	  return;
  }
  ASSERT_ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  p->set_timestamp_anno(Timestamp::now());
  
  // fill in ethernet header 
  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _eth.data(), 6);

  link_probe *lp = (struct link_probe *) (p->data() + sizeof(click_ether));
  lp->_version = _ett_version;
  lp->_ip = _ip.addr();
  lp->_seq = p->timestamp_anno().sec();
  lp->_period = _period;
  lp->_tau = _tau;
  lp->_sent = _sent;
  lp->_flags = 0;
  lp->_rate = rate;
  lp->_size = size;
  lp->_num_probes = _ads_rs.size();
  
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
    lp->_flags |= PROBE_AVAILABLE_RATES;
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
		    _neighbors[_next_neighbor_to_ad].s().c_str(),
		    _next_neighbor_to_ad);
    } else {

      int size = probe->_probe_types.size()*sizeof(link_info) + sizeof(link_entry);
      if (ptr + size > end) {
	break;
      }
      num_entries++;
      link_entry *entry = (struct link_entry *)(ptr); 
      entry->_ip = probe->_ip;
      entry->_seq = probe->_seq;	
      if ((uint32_t) probe->_ip > (uint32_t) _ip) {
	entry->_seq = lp->_seq;
      }
      entry->_num_rates = probe->_probe_types.size();
      ptr += sizeof(link_entry);

      Vector<RateSize> rates;
      Vector<int> fwd;
      Vector<int> rev;

      for (int x = 0; x < probe->_probe_types.size(); x++) {
	RateSize rs = probe->_probe_types[x];
	link_info *lnfo = (struct link_info *) (ptr + x*sizeof(link_info));
	lnfo->_size = rs._size;
	lnfo->_rate = rs._rate;
	lnfo->_fwd = probe->fwd_rate(rs._rate, rs._size);
	lnfo->_rev = probe->rev_rate(_start, rs._rate, rs._size);
	
	rates.push_back(rs);
	fwd.push_back(lnfo->_fwd);
	rev.push_back(lnfo->_rev);
      }
      update_link(_ip, entry->_ip, rates, fwd, rev, entry->_seq);

      ptr += probe->_probe_types.size()*sizeof(link_info);
    }

  }
  
  lp->_num_links = num_entries;
  lp->_psz = sizeof(link_probe) + lp->_num_links*sizeof(link_entry);
  lp->_cksum = 0;
  lp->_cksum = click_in_cksum((unsigned char *) lp, lp->_psz);
  

  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();
  ceh->magic = WIFI_EXTRA_MAGIC;
  ceh->rate = rate;
  checked_output_push(0, p);
}

int
ETTStat::initialize(ErrorHandler *errh)
{
  if (noutputs() > 0) {
    if (!_eth) 
      return errh->error("Source Ethernet address must be specified to send probes");
    
    _timer.initialize(this);    
    _next = Timestamp::now();

    unsigned max_jitter = _period / 10;
    add_jitter(max_jitter, &_next);

    _timer.schedule_at(_next);
  }

  reset();
  return 0;
}



Packet *
ETTStat::simple_action(Packet *p)
{

  
  Timestamp now = Timestamp::now();

  unsigned min_sz = sizeof(click_ether) + sizeof(link_probe);
  if (p->length() < min_sz) {
	  click_chatter("%{element}: packet is too small", this);
    p->kill(); 
    return 0;
  }

  click_ether *eh = (click_ether *) p->data();

  if (ntohs(eh->ether_type) != _et) {
    click_chatter("%{element}: got non-ETTStat packet type", this);
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
		     EtherAddress(eh->ether_shost).s().c_str());
    }

    
    p->kill();
    return 0;
  }


  if (click_in_cksum((unsigned char *) lp, lp->_psz) != 0) {
	  click_chatter("%{element}: failed checksum", this);
    p->kill();
    return 0;
  }


  if (p->length() < lp->_psz + sizeof(click_ether)) {
    click_chatter("%{element}: packet is smaller (%d) than it claims (%u)",
		  this, p->length(), lp->_psz);
  }


  IPAddress ip = IPAddress(lp->_ip);

  if (ip == _ip) {
    click_chatter("%{element} got own packet %s\n",
		  this,
		  _ip.s().c_str());
    p->kill();
    return 0;
  }
  if (_arp_table) {
    _arp_table->insert(ip, EtherAddress(eh->ether_shost));
    _rev_arp.insert(EtherAddress(eh->ether_shost), ip);
  }
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();
  uint8_t rate = ceh->rate;

  if (ceh->rate != lp->_rate) {
    click_chatter("%{element} packet says rate %d is %d\n",
		  this,
		  lp->_rate,
		  ceh->rate);
    p->kill();
    return 0;
  }

  probe_t probe(now, lp->_seq, lp->_rate, lp->_size, ceh->rssi, ceh->silence);
  int new_period = lp->_period;
  probe_list_t *l = _bcast_stats.findp(ip);
  int x = 0;
  if (!l) {
    _bcast_stats.insert(ip, probe_list_t(ip, new_period, lp->_tau));
    l = _bcast_stats.findp(ip);
    l->_sent = 0;
    /* add into the neighbors vector */
    _neighbors.push_back(ip);
  } else if (l->_period != new_period) {
    click_chatter("%{element}: %s has changed its link probe period from %u to %u; clearing probe info",
		  this, ip.s().c_str(), l->_period, new_period);
    l->_probes.clear();
  } else if (l->_tau != lp->_tau) {
    click_chatter("%{element}: %s has changed its link tau from %u to %u; clearing probe info",
		  this, ip.s().c_str(), l->_tau, lp->_tau);
    l->_probes.clear();
  }

  if (lp->_sent < (unsigned)l->_sent) {
    click_chatter("%{element}: %s has reset; clearing probe info",
		  this, ip.s().c_str());
    l->_probes.clear();
  }
  
  RateSize rs = RateSize(rate, lp->_size);
  l->_period = new_period;
  l->_tau = lp->_tau;
  l->_sent = lp->_sent;
  l->_last_rx = now;
  l->_num_probes = lp->_num_probes;
  l->_probes.push_back(probe);
  l->_seq = probe._seq;

  /* keep stats for at least the averaging period */
  while ((unsigned) l->_probes.size() &&
	 now._sec - l->_probes[0]._when._sec > (signed) (1 + (l->_tau / 1000)))
    l->_probes.pop_front();
  

  
  for (x = 0; x < l->_probe_types.size(); x++) {
    if (rs == l->_probe_types[x]) {
      break;
    }
  }
  
  if (x == l->_probe_types.size()) {
    l->_probe_types.push_back(rs);
    l->_fwd_rates.push_back(0);
  }

  uint8_t *ptr =  (uint8_t *) (lp + 1);

  uint8_t *end  = (uint8_t *) p->data() + p->length();


  if (lp->_flags &= PROBE_AVAILABLE_RATES) {
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
  while (ptr < end && (unsigned) link_number < lp->_num_links) {
    link_number++;
    link_entry *entry = (struct link_entry *)(ptr); 
    IPAddress neighbor = entry->_ip;
    int num_rates = entry->_num_rates;
    if (0) {
      click_chatter("%{element} on link number %d / %d: neighbor %s, num_rates %d\n",
		    this,
		    link_number,
		    lp->_num_links,
		    neighbor.s().c_str(),
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
		      ip.s().c_str(),
		      neighbor.s().c_str(),
		      nfo->_size,
		      nfo->_rate,
		      nfo->_fwd,
		      nfo->_rev);
      }
      
      RateSize rs = RateSize(nfo->_rate, nfo->_size);
      /* update other link stuff */
      rates.push_back(rs);
      fwd.push_back(nfo->_fwd);
      if (neighbor == _ip) {
	rev.push_back(l->rev_rate(_start, rates[x]._rate, rates[x]._size));
      } else {
	rev.push_back(nfo->_rev);
      }

      if (neighbor == _ip) {
	/* set the fwd rate */
	for (int x = 0; x < l->_probe_types.size(); x++) {
	  if (rs == l->_probe_types[x]) {
	    l->_fwd_rates[x] = nfo->_rev;
	    
	    break;
	  }
	}
      }
    }
    int seq = entry->_seq;
    if (neighbor == ip && 
	((uint32_t) neighbor > (uint32_t) _ip)) {
      seq = now._sec;
    }
    update_link(ip, neighbor, rates, fwd, rev, seq);
    ptr += num_rates * sizeof(struct link_info);
    
  }

  p->kill();
  return 0;
}


static int ipaddr_sorter(const void *va, const void *vb) {
    IPAddress *a = (IPAddress *)va, *b = (IPAddress *)vb;
    if (a->addr() == b->addr()) {
      return 0;
    } 
    return (ntohl(a->addr()) < ntohl(b->addr())) ? -1 : 1;
}

String
ETTStat::read_bcast_stats()
{
  Vector<IPAddress> ip_addrs;
  
  for (ProbeMap::const_iterator i = _bcast_stats.begin(); i; i++) 
    ip_addrs.push_back(i.key());

  Timestamp now = Timestamp::now();

  StringAccum sa;

  click_qsort(ip_addrs.begin(), ip_addrs.size(), sizeof(IPAddress), ipaddr_sorter);

  for (int i = 0; i < ip_addrs.size(); i++) {
    IPAddress ip  = ip_addrs[i];
    probe_list_t *pl = _bcast_stats.findp(ip);
    //sa << _ip << " " << _eth << " ";
    sa << ip;
    if (_arp_table) {
      EtherAddress eth_dest = _arp_table->lookup(ip);
      if (eth_dest) {
	sa << " " << eth_dest.s().c_str();
      } else {
	sa << " ?";
      }
    } else {
      sa << " ?";
    }


    sa << " seq " << pl->_seq;
    sa << " period " << pl->_period;
    sa << " tau " << pl->_tau;
    sa << " sent " << pl->_sent;
    sa << " last_rx " << now - pl->_last_rx;
    sa << "\n";



    for (int x = 0; x < _ads_rs.size(); x++) {
	    int rate = _ads_rs[x]._rate;
	    int size = _ads_rs[x]._size;
	    int rev = pl->rev_rate(_start, rate, size);
	    int fwd = pl->fwd_rate(rate, size);
	    int rssi = pl->rev_rssi(rate, size);
	    int noise = pl->rev_noise(rate, size);

	    sa << ip;
	    if (_arp_table) {
		    EtherAddress eth_dest = _arp_table->lookup(ip);
		    if (eth_dest) {
			    sa << " " << eth_dest.s();
		    } else {
			    sa << " ?";
		    }
	    } else {
		    sa << " ?";
	    }
	    sa << " [ " << rate << " " << size << " ";
	    sa << fwd << " " << rev << " ";
	    sa << rssi << " " << noise << " ]";
	    sa << "\n";
    }

  }

  return sa.take_string();
}


String 
ETTStat::bad_nodes() {

  StringAccum sa;
  for (BadTable::const_iterator i = _bad_table.begin(); i; i++) {
    uint8_t version = i.value();
    EtherAddress dst = i.key();
    sa << this << " eth " << dst.s().c_str() << " version " << (int) version << "\n";
  }

  return sa.take_string();
}


void
ETTStat::clear_stale() 
{
  Vector<IPAddress> new_neighbors;

  Timestamp now = Timestamp::now();
  for (int x = 0; x < _neighbors.size(); x++) {
    IPAddress n = _neighbors[x];
    probe_list_t *l = _bcast_stats.findp(n);
    if (!l || 
	(unsigned) now._sec - l->_last_rx._sec > 2 * l->_tau/1000) {
      click_chatter("%{element} clearing stale neighbor %s age %d\n ",
		    this, n.s().c_str(),
		    now._sec - l->_last_rx._sec);
      _bcast_stats.remove(n);
    } else {
      new_neighbors.push_back(n);
    }
  }

  _neighbors.clear();
  for (int x = 0; x < new_neighbors.size(); x++) {
    _neighbors.push_back(new_neighbors[x]);
  }

}
void
ETTStat::reset()
{
  _neighbors.clear();
  _bcast_stats.clear();
  _rev_arp.clear();
  _seq = 0;
  _sent = 0;
  _start = Timestamp::now();
}


void
ETTStat::add_handlers()
{
  add_read_handler("bcast_stats", ETTStat_read_param, (void *) H_BCAST_STATS);
  add_read_handler("bad_version", ETTStat_read_param, (void *) H_BAD_VERSION);
  add_read_handler("ip", ETTStat_read_param, (void *) H_IP);
  add_read_handler("tau", ETTStat_read_param, (void *) H_TAU);
  add_read_handler("period", ETTStat_read_param, (void *) H_PERIOD);
  add_read_handler("probes", ETTStat_read_param, (void *) H_PROBES);

  add_write_handler("reset", ETTStat_write_param, (void *) H_RESET);
  add_write_handler("tau", ETTStat_write_param, (void *) H_TAU);
  add_write_handler("period", ETTStat_write_param, (void *) H_PERIOD);
  add_write_handler("probes", ETTStat_write_param, (void *) H_PROBES);

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
