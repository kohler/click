/*
 * srcrstat.{cc,hh} -- track per-link delivery rates.
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
#include "srcrstat.hh"

CLICK_DECLS


#define MIN(x,y)      ((x)<(y) ? (x) : (y))
#define MAX(x,y)      ((x)>(y) ? (x) : (y));


SrcrStat::SrcrStat()
  : _window(100), 
    _tau(10000), 
    _period(1000), 
    _probe_size(1000),
    _seq(0), 
    _sent(0),
    _link_table(0),
    _arp_table(0),
    _send_timer(0)
{
  MOD_INC_USE_COUNT;
  add_input();
}

SrcrStat::~SrcrStat()
{
  MOD_DEC_USE_COUNT;
}

SrcrStat *
SrcrStat::clone() const
{
  return new SrcrStat();
}

void
SrcrStat::notify_noutputs(int n) 
{
  set_noutputs(n > 0 ? 1 : 0);  
}


int
SrcrStat::configure(Vector<String> &conf, ErrorHandler *errh)
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
			"LT", cpElement, "LinkTable element", &_link_table,
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

  if (_link_table && _link_table->cast("LinkTable") == 0) {
    return errh->error("LinkTable element is not a LinkTable");
  }
  if (_arp_table && _arp_table->cast("ARPTable") == 0) {
    return errh->error("ARPTable element is not a ARPyTable");
  }
  return res;
}

void
SrcrStat::send_hook() 
{
  WritablePacket *p = Packet::make(_probe_size + 2); // +2 for alignment
  if (p == 0) {
    click_chatter("SrcrStat %s: cannot make packet!", id().cc());
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
  unsigned min_packet_sz = sizeof(click_ether) + sizeof(struct link_probe);
  unsigned max_entries = (_probe_size - min_packet_sz) / sizeof(struct link_entry);

  if (_sent <= (_tau / _period)+1) {
    _sent++;
  }

  Vector<link_entry> to_send;
  for (ProbeMap::const_iterator i = _bcast_stats.begin(); i > 0; i++) {
    probe_list_t val = i.value();
    struct timeval old_entry_timeout;
    struct timeval old_entry_expire;
    old_entry_timeout.tv_sec = 30;
    old_entry_timeout.tv_usec = 30;
    timeradd(&val.last_rx, &old_entry_timeout, &old_entry_expire);
    
    if (timercmp(&now, &old_entry_expire, <)) {
      to_send.push_back(link_entry(val.fwd, val.rev_rate(_start), val.ip));
      
    }
  }


  static bool size_warning = false;
  if (!size_warning && max_entries < (unsigned) to_send.size()) {
    size_warning = true;
    click_chatter("SrcrStat %s: WARNING, probe packet is too small to contain all link stats", id().cc());
  }




  unsigned num_entries = max_entries < (unsigned) to_send.size() ? max_entries : to_send.size();
  link_probe *lp = (struct link_probe *) (p->data() + sizeof(click_ether));
  lp->ip = _ip.addr();
  lp->seq_no = _seq;
  lp->period = _period;
  lp->tau = _tau;
  lp->sent = _sent;
  lp->num_links = num_entries;

  _seq++;
  


  link_entry *entry = (struct link_entry *)(lp+1); 
  for (unsigned x = 0; x < num_entries; x++) {
    entry->ip = to_send[x].ip;
    entry->fwd = to_send[x].fwd;
    entry->rev = to_send[x].rev;
    entry = (entry+1);
  }

  lp->psz = sizeof(link_probe) + lp->num_links*sizeof(link_entry);
  lp->cksum = 0;
  lp->cksum = click_in_cksum((unsigned char *) lp, lp->psz);
  
  unsigned max_jitter = _period / 10;
  long r2 = random();
  unsigned j = (unsigned) ((r2 >> 1) % (max_jitter + 1));
  unsigned int delta_us = 1000 * ((r2 & 1) ? _period - j : _period + j);
  _next_bcast.tv_usec += delta_us;
  _next_bcast.tv_sec +=  _next_bcast.tv_usec / 1000000;
  _next_bcast.tv_usec = (_next_bcast.tv_usec % 1000000);
  _send_timer->schedule_at(_next_bcast);

  checked_output_push(0, p);
}

int
SrcrStat::initialize(ErrorHandler *errh)
{
  if (noutputs() > 0) {
    if (!_eth) 
      return errh->error("Source Ethernet address must be specified to send probes");
    _send_timer = new Timer(static_send_hook, this);
    _send_timer->initialize(this);
    click_gettimeofday(&_next_bcast);
    _next_bcast.tv_sec++;
    _send_timer->schedule_at(_next_bcast);
  }
  click_gettimeofday(&_start);
  return 0;
}



Packet *
SrcrStat::simple_action(Packet *p)
{
  unsigned min_sz = sizeof(click_ether) + sizeof(link_probe);
  if (p->length() < min_sz) {
    click_chatter("SrcrStat %s: packet is too small", id().cc());
    p->kill(); 
    return 0;
  }

  click_ether *eh = (click_ether *) p->data();

  if (ntohs(eh->ether_type) != _et) {
    click_chatter("SrcrStat %s: got non-SrcrStat packet type", id().cc());
    p->kill();
    return 0;
  }
  link_probe *lp = (link_probe *)(p->data() + sizeof(click_ether));


  if (click_in_cksum((unsigned char *) lp, lp->psz) != 0) {
    click_chatter("SrcrStat %s: failed checksum", id().cc());
    p->kill();
    return 0;
  }


  if (p->length() < lp->psz + sizeof(click_ether)) {
    click_chatter("SrcrStat %s: packet is smaller (%d) than it claims (%u)",
		  id().cc(), p->length(), lp->psz);
  }


  IPAddress ip = IPAddress(lp->ip);
  if (_arp_table) {
    _arp_table->insert(ip, EtherAddress(eh->ether_shost));
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
  } else if (l->period != new_period) {
    click_chatter("SrcrStat %s: %s has changed its link probe period from %u to %u; clearing probe info\n",
		  id().cc(), ip.s().cc(), l->period, new_period);
    l->probes.clear();
  }

  if (lp->sent < (unsigned)l->sent) {
    click_chatter("SrcrStat %s: %s has reset; clearing probe info\n",
		  id().cc(), ip.s().cc());
    l->probes.clear();
  }

  l->period = new_period;
  l->tau = lp->tau;
  l->sent = lp->sent;
  l->last_rx = now;
  l->probes.push_back(probe);

  /* only keep stats for last _window *unique* sequence numbers */
  while ((unsigned) l->probes.size() > _window) 
    l->probes.pop_front();



  // look in received packet for info about our outgoing link
  unsigned int max_entries = (p->length() - sizeof(*eh) - sizeof(link_probe)) / sizeof(link_entry);
  unsigned int num_entries = lp->num_links;
  if (num_entries > max_entries) {
    click_chatter("SrcrStat %s: WARNING, probe packet from %s contains fewer link entries (at most %u) than claimed (%u)", 
		  id().cc(), IPAddress(lp->ip).s().cc(), max_entries, num_entries);
    num_entries = max_entries;
  }


  const unsigned char *d = p->data() + sizeof(click_ether) + sizeof(link_probe);
  for (unsigned i = 0; i < num_entries; i++, d += sizeof(link_entry)) {
    link_entry *le = (struct link_entry *) d;
    int etx = 0;
    if (IPAddress(le->ip) == _ip) {
      l->fwd = le->rev;
      etx = get_etx(le->rev, l->rev_rate(_start));
    } else {
      etx = get_etx(le->rev, le->fwd);
    }

    if (_link_table) {
      _link_table->update_link(IPAddress(le->ip), ip, etx);
      _link_table->update_link(ip, IPAddress(le->ip), etx);
    }
  }

  p->kill();
  return 0;
}



int 
SrcrStat::get_etx(int fwd, int rev) {

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
int 
SrcrStat::get_etx(IPAddress ip) {
  probe_list_t *l = _bcast_stats.findp(ip);
  if (l) {
    return get_etx(l->fwd, l->rev_rate(_start));
  }
  return 7777;
}

String
SrcrStat::read_window(Element *xf, void *)
{
  SrcrStat *f = (SrcrStat *) xf;
  return String(f->_window) + "\n";
}

String
SrcrStat::read_period(Element *xf, void *)
{
  SrcrStat *f = (SrcrStat *) xf;
  return String(f->_period) + "\n";
}

String
SrcrStat::read_tau(Element *xf, void *)
{
  SrcrStat *f = (SrcrStat *) xf;
  return String(f->_tau) + "\n";
}

String
SrcrStat::read_bcast_stats(Element *xf, void *)
{
  SrcrStat *e = (SrcrStat *) xf;

  typedef BigHashMap<IPAddress, bool> IPMap;
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
    sa << " fwd " << pl->fwd;
    int rev = pl->rev_rate(e->_start);
    sa << " rev " << rev;
    sa << " etx " << e->get_etx(pl->fwd, rev);
    sa << " last_rx " << now - pl->last_rx;
    sa << "\n";
    
  }

  return sa.take_string();
}


int
SrcrStat::write_window(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  SrcrStat *e = (SrcrStat *) el;
  if (!cp_unsigned(arg, &e->_window))
    return errh->error("window must be >= 0");
  return 0;
}

int
SrcrStat::write_period(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  SrcrStat *e = (SrcrStat *) el;
  if (!cp_unsigned(arg, &e->_period))
    return errh->error("period must be >= 0");

  return 0;
}

int
SrcrStat::write_tau(const String &arg, Element *el, 
		       void *, ErrorHandler *errh)
{
  SrcrStat *e = (SrcrStat *) el;
  if (!cp_unsigned(arg, &e->_tau))
    return errh->error("tau must be >= 0");
  return 0;
}


void
SrcrStat::add_handlers()
{
  add_read_handler("bcast_stats", read_bcast_stats, 0);

  add_read_handler("window", read_window, 0);
  add_read_handler("tau", read_tau, 0);
  add_read_handler("period", read_period, 0);

  add_write_handler("window", write_window, 0);
  add_write_handler("tau", write_tau, 0);
  add_write_handler("period", write_period, 0);
}



EXPORT_ELEMENT(SrcrStat)

#include <click/bighashmap.cc>
#include <click/dequeue.cc>
#include <click/vector.cc>
template class DEQueue<SrcrStat::probe_t>;
template class BigHashMap<IPAddress, SrcrStat::probe_list_t>;
CLICK_ENDDECLS
