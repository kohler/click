/*
 * FlashFlood.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachusflashfloods Institute of Technology
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
#include "flashflood.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "ettmetric.hh"
#include "srpacket.hh"
CLICK_DECLS

FlashFlood::FlashFlood()
  :  Element(2,2),
     _en(),
     _et(0),
     _ett_metric(0),
     _packets_originated(0),
     _packets_tx(0),
     _packets_rx(0)
{
  MOD_INC_USE_COUNT;

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

FlashFlood::~FlashFlood()
{
  MOD_DEC_USE_COUNT;
}

int
FlashFlood::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  _history = 100;
  _min_p = 50;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
                    "BCAST_IP", cpIPAddress, "IP address", &_bcast_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "ETT", cpElement, "ETTMetric", &_ett_metric,
		    "MIN_P", cpUnsigned, "Min P", &_min_p,
		    /* below not required */
		    "DEBUG", cpBool, "Debug", &_debug,
		    "HISTORY", cpUnsigned, "history", &_history,
                    0);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_bcast_ip) 
    return errh->error("BCAST_IP not specified");
  if (!_en) 
    return errh->error("ETH not specified");
  if (_ett_metric == 0) 
    return errh->error("no ETTMetric element specified");
  if (_ett_metric->cast("ETTMetric") == 0)
    return errh->error("ETTMetric element is not a ETTMetric");

  return ret;
}

FlashFlood *
FlashFlood::clone () const
{
  return new FlashFlood;
}

int
FlashFlood::initialize (ErrorHandler *)
{
  return 0;
}

void
FlashFlood::forward(Broadcast *bcast) {
  Packet *p_in = bcast->_p;
  click_ether *eh_in = (click_ether *) p_in->data();
  struct srpacket *pk_in = (struct srpacket *) (eh_in+1);

  int hops = 1;
  int len = 0;
  if (bcast->_originated) {
    hops = 1;
    len = srpacket::len_with_data(hops, p_in->length());
  } else {
    hops = pk_in->num_hops() + 1;
    len = srpacket::len_with_data(hops, pk_in->data_len());
  }

  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if (p == 0)
    return;

  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);

  memset(pk, '\0', len);

  pk->_version = _sr_version;
  pk->_type = PT_DATA;
  pk->_flags = 0;
  pk->_qdst = _bcast_ip;
  pk->set_num_hops(hops);
  for (int x = 0; x < hops - 1; x++) {
    pk->set_hop(x, pk_in->get_hop(x));
  }
  pk->set_hop(hops - 1,_ip);
  pk->set_next(hops);
  pk->set_seq(bcast->_seq);
  if (bcast->_originated) {
    memcpy(pk->data(), p_in->data(), p_in->length());
    pk->set_data_len(p_in->length());

  } else {
    memcpy(pk->data(), pk_in->data(), pk_in->data_len());
    pk->set_data_len(pk_in->data_len());
  }
  
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);
  memset(eh->ether_dhost, 0xff, 6);
  output(0).push(p);

  bcast->_sent = true;
  bcast->_num_tx++;
  _packets_tx++;
  bcast->del_timer();
  update_probs(_ip, bcast);
  reschedule_bcast(bcast);

}

void
FlashFlood::forward_hook() 
{
  struct timeval now;
  click_gettimeofday(&now);
  for (int x = 0; x < _packets.size(); x++) {
    if (timercmp(&_packets[x]._to_send, &now, <=)) {
      /* this timer has expired */
      if (!_packets[x]._sent) {
	/* we haven't sent this packet yet */
	forward(&_packets[x]);
      }
    }
  }
}


void
FlashFlood::trim_packets() {
  /* only keep track of the last _max_packets */
  while ((_packets.size() > _history)) {
    /* unschedule and remove packet*/
    if (_debug) {
      click_chatter("%{element} removing seq %d\n",
		    this,
		    _packets[0]._seq);
    }
    _packets[0].del_timer();
    if (_packets[0]._p) {
      _packets[0]._p->kill();
    }
    _packets.pop_front();
  }
}
void
FlashFlood::push(int port, Packet *p_in)
{
  struct timeval now;
  click_gettimeofday(&now);
  
  if (port == 1) {
    _packets_originated++;
    /* from me */
    int index = _packets.size();
    _packets.push_back(Broadcast());
    _packets[index]._seq = random();
    _packets[index]._originated = true;
    _packets[index]._p = p_in;
    _packets[index]._num_rx = 0;
    _packets[index]._num_tx = 0;
    _packets[index]._first_rx = now;
    _packets[index]._sent = false;
    _packets[index].t = NULL;
    _packets[index]._to_send = now;
    forward(&_packets[index]);
  } else {
    _packets_rx++;

    click_ether *eh = (click_ether *) p_in->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
    
    uint32_t seq = pk->seq();
    
    int index = -1;
    for (int x = 0; x < _packets.size(); x++) {
      if (_packets[x]._seq == seq) {
	index = x;
	break;
      }
    }

    if (index == -1) {
      /* haven't seen this packet before */
      index = _packets.size();
      _packets.push_back(Broadcast());
      _packets[index]._seq = seq;
      _packets[index]._originated = false;
      _packets[index]._p = p_in;
      _packets[index]._num_rx = 0;
      _packets[index]._num_tx = 0;
      _packets[index]._first_rx = now;
      _packets[index]._sent = false;
      _packets[index].t = NULL;

      /* clone the packet and push it out */
      Packet *p_out = p_in->clone();
      output(1).push(p_out);
    }
    _packets[index]._num_rx++;
    IPAddress src = pk->get_hop(pk->num_hops() - 1);
    if (_debug) {
      click_chatter("%{element} got packet from %s",
		    this,
		    src.s().cc());
    }
    update_probs(src, &_packets[index]);
    reschedule_bcast(&_packets[index]);
  }
  trim_packets();
}

void 
FlashFlood::update_probs(IPAddress src, Broadcast *bcast) {

    Vector<IPAddress> neighbors = _ett_metric->get_neighbors();
    for (int x = 0; x < neighbors.size(); x++) {
      /*
       * p(got packet ever) = (1 - p(never))
       *                    = (1 - (p(not before))*(p(not now)))
       *                    = (1 - (1 - p(before))*(1 - p(now)))
       *
       */
      if (src == neighbors[x]) {
	/* they sent it */
	bcast->_node_to_prob.insert(neighbors[x], 100);
      } else {
	int p_now = _ett_metric->get_delivery_rate(1, src, neighbors[x]);
	int *p_before_ptr = bcast->_node_to_prob.findp(neighbors[x]);
	int p_before = (p_before_ptr) ? *p_before_ptr : 0;
	int p_ever = (100 - (((100 - p_before) * (100 - p_now))/100));
	bcast->_node_to_prob.insert(neighbors[x], p_ever);
      }
    }
}

void
FlashFlood::reschedule_bcast(Broadcast *bcast) {    
    Vector<IPAddress> neighbors = _ett_metric->get_neighbors();
    int min_p_ever = 100;
    int expected_rx = 0;
    for (int x = 0; x < neighbors.size(); x++) {
      int *p_ever_ptr = bcast->_node_to_prob.findp(neighbors[x]);
      sr_assert(p_ever_ptr);
      if (!p_ever_ptr) {
	continue;
      }
      int p_ever = *p_ever_ptr;
      min_p_ever = min(min_p_ever, p_ever);
      expected_rx += ((100 - p_ever) * 
		      _ett_metric->get_delivery_rate(1, _ip, neighbors[x]))/100;

      if (_debug) {
	click_chatter("%{element} reschedule_bcast neighbor %s, p_ever %d min %d\n",
		      this,
		      neighbors[x].s().cc(),
		      p_ever,
		      min_p_ever);
      }
    }
    
    if (_debug) {
      click_chatter("%{element} reschedule for seq %d: min_p_ever %d expected_rx %d\n",
		    this,
		    bcast->_seq,
		    min_p_ever,
		    expected_rx);
    }
    if (min_p_ever < _min_p) {
      int max_delay_ms = 1000;
      sr_assert(expected_rx);
      if (expected_rx) {
	max_delay_ms = 100 * 1000 / expected_rx;
      }
      
      int delay_time = (random() % max_delay_ms) + 1;

      if (_debug) {
	click_chatter("%{element} delay time is %d / max %d ms\n",
		      this,
		      delay_time,
		      max_delay_ms);
      }
      sr_assert(delay_time > 0);
      struct timeval now;
      click_gettimeofday(&now);
      struct timeval delay;
      delay.tv_sec = 0;
      delay.tv_usec = delay_time*1000;
      timeradd(&now, &delay, &bcast->_to_send);
      bcast->del_timer();
      /* schedule timer */
      bcast->_sent = false;
      bcast->t = new Timer(static_forward_hook, (void *) this);
      bcast->t->initialize(this);
      bcast->t->schedule_at(bcast->_to_send);
    }

}


String
FlashFlood::static_print_stats(Element *f, void *)
{
  FlashFlood *d = (FlashFlood *) f;
  return d->print_stats();
}

String
FlashFlood::print_stats()
{
  StringAccum sa;

  sa << "originated " << _packets_originated << "\n";
  sa << "tx " << _packets_tx << "\n";
  sa << "rx " << _packets_rx << "\n";
  return sa.take_string();
}

int
FlashFlood::static_write_debug(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  FlashFlood *n = (FlashFlood *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`debug' must be a boolean");

  n->_debug = b;
  return 0;
}
String
FlashFlood::static_print_debug(Element *f, void *)
{
  StringAccum sa;
  FlashFlood *d = (FlashFlood *) f;
  sa << d->_debug << "\n";
  return sa.take_string();
}

String
FlashFlood::print_packets()
{
  StringAccum sa;
  for (int x = 0; x < _packets.size(); x++) {
    sa << "seq " << _packets[x]._seq;
    sa << " originated " << _packets[x]._originated;
    sa << " num_rx " << _packets[x]._num_rx;
    sa << " first_rx " << _packets[x]._first_rx;
    sa << " num_tx " << _packets[x]._num_tx;
    sa << " sent " << _packets[x]._sent;
    sa << " to_send " << _packets[x]._to_send;
    sa << "\n";
  }
  return sa.take_string();
}
String
FlashFlood::static_print_packets(Element *f, void *)
{
  FlashFlood *d = (FlashFlood *) f;
  return d->print_packets();
}

void
FlashFlood::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_read_handler("debug", static_print_debug, 0);
  add_read_handler("packets", static_print_packets, 0);

  add_write_handler("debug", static_write_debug, 0);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/dequeue.cc>
#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<FlashFlood::Broadcast>;
template class HashMap<IPAddress, int>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(FlashFlood)
