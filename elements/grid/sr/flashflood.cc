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
     _link_table(0),
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
  _lossy = true;
  _threshold = 100;
  _neighbor_threshold = 66;
  _pick_slots = false;
  _slots_nweight = false;
  _slots_erx = false;
  _slot_time_ms = 15;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
                    "BCAST_IP", cpIPAddress, "IP address", &_bcast_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "LT", cpElement, "Linktable", &_link_table,
		    "MIN_P", cpInteger, "Min P", &_min_p,
		    /* below not required */
		    "DEBUG", cpBool, "Debug", &_debug,
		    "HISTORY", cpUnsigned, "history", &_history,
		    "LOSSY", cpBool, "mist", &_lossy,
		    "THRESHOLD", cpInteger, "Threshold", &_threshold,
		    "NEIGHBOR_THRESHOLD", cpInteger, "Neighbor Threshold", &_neighbor_threshold,
		    "PICK_SLOTS", cpBool, "Wait time selection", &_pick_slots,
		    "SLOTS_NEIGHBOR_WEIGHT", cpBool, "use max neighbor weight for slots", &_slots_nweight,
		    "SLOTS_EXPECTED_RX", cpBool, "foo", &_slots_erx,
		    "SLOT_TIME_MS", cpInteger, "time (in ms) for a slot", &_slot_time_ms,
                    0);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_bcast_ip) 
    return errh->error("BCAST_IP not specified");
  if (!_en) 
    return errh->error("ETH not specified");
  if (_link_table == 0) 
    return errh->error("no LinkTable element specified");
  if (_link_table->cast("LinkTable") == 0)
    return errh->error("LinkTable element is not a LinkTable");

  if (_pick_slots && !(_slots_erx ^ _slots_nweight)) {
    return errh->error("One of SLOTS_NEIGHBOR_WEIGHT or SLOTS_EXPECTED_RX may be true");
  }

  if (_slot_time_ms < 0) {
    return errh->error("SLOT_TIME_MS must be positive\n");
  }
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

FlashFlood::SeqProbMap *
FlashFlood::findmap(uint32_t seq) 
{
  int index = -1;
  for (int x = 0; x < _mappings.size(); x++) {
    if (_mappings[x]._seq == seq) {
      index = x;
      break;
    }
  }
  
  if (index == -1) {
    return 0;
  }
  return &_mappings[index];
}


bool
FlashFlood::get_prob(uint32_t seq, IPAddress src, int *p)
{
  if (!p) {
    click_chatter("%{element} error, seq %d p is null\n",
		  this,
		  seq);
    return false;
  }
  SeqProbMap *m = findmap(seq);
  if (!m) {
    click_chatter("%{element} error, couldn't find seq %d in get_prob\n",
		  this,
		  seq);
    return false;
  }

  int *p_ptr = m->_node_to_prob.findp(src);
  *p = (p_ptr) ? *p_ptr : 0;
  return true;
  
}

bool
FlashFlood::set_prob(uint32_t seq, IPAddress src, int p)
{
  SeqProbMap *m = findmap(seq);
  if (!m) {
    click_chatter("%{element} error, couldn't find seq %d in set_prob\n",
		  this,
		  seq);
    return false;
  }
  m->_node_to_prob.insert(src, p);
  return true;
}

int 
FlashFlood::expected_rx(uint32_t seq, IPAddress src) {


  SeqProbMap *m = findmap(seq);
  if (!m) {
    click_chatter("%{element} error, couldn't find seq %d in expected_rx\n",
		  this,
		  seq);
    return 0;

  }

  Vector<IPAddress> neighbors = _link_table->get_neighbors(src);
  int my_expected_rx = 0;
  
  for (int x = 0; x < neighbors.size(); x++) {
    if (neighbors[x] == src || neighbors[x] == _ip) {
      continue;
    }
    int *p_ever_ptr = m->_node_to_prob.findp(neighbors[x]);
    if (!p_ever_ptr) {
      m->_node_to_prob.insert(neighbors[x], 0);
      p_ever_ptr = m->_node_to_prob.findp(neighbors[x]);
    }
    int p_ever = *p_ever_ptr;
    if (p_ever > 100) {
      click_chatter("%{element} p_ever is %d\n",
		    this,
		    p_ever);
      sr_assert(false);
    }
    int metric = get_link_prob(src, neighbors[x]);
    int neighbor_expected_rx = ((100 - p_ever) * metric)/100;
    my_expected_rx += neighbor_expected_rx;
  }

  return my_expected_rx;
}


int 
FlashFlood::neighbor_weight(IPAddress src) {
  Vector<IPAddress> neighbors = _link_table->get_neighbors(src);
  int src_neighbor_weight = 0;
  
  for (int x = 0; x < neighbors.size(); x++) {
    int metric = get_link_prob(src, neighbors[x]);
    src_neighbor_weight += metric;
  }

  return src_neighbor_weight;
}

void
FlashFlood::forward(Broadcast *bcast) {
  
  int my_expected_rx = expected_rx(bcast->_seq, _ip);

  if (my_expected_rx < _threshold) {
    if (_debug) {
      click_chatter("%{element} seq %d sender %s my_expected_rx %d not_sending\n",
		    this,
		    bcast->_seq,
		    bcast->_rx_from.s().cc(),
		    my_expected_rx);
    }

    bcast->_sent = true;
    bcast->_scheduled = false;
    bcast->del_timer();
    return;
  }
  if (_debug) {
    click_chatter("%{element} seq %d sender %s my_expected_rx %d sending\n",
		  this,
		  bcast->_seq,
		  bcast->_rx_from.s().cc(),
		  my_expected_rx);
  }


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
  bcast->_scheduled = false;
  bcast->_num_tx++;
  _packets_tx++;
  bcast->del_timer();
  update_probs(bcast->_seq, _ip);
}

void
FlashFlood::forward_hook() 
{
  struct timeval now;
  click_gettimeofday(&now);
  for (int x = 0; x < _packets.size(); x++) {
    if (timercmp(&_packets[x]._to_send, &now, <=)) {
      /* this timer has expired */
      if (!_packets[x]._sent && _packets[x]._scheduled) {
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
      click_chatter("%{element} removing packet seq %d\n",
		    this,
		    _packets[0]._seq);
    }
    _packets[0].del_timer();
    if (_packets[0]._p) {
      _packets[0]._p->kill();
    }
    _packets.pop_front();
  }

  while ((_mappings.size() > _history)) {
    /* unschedule and remove packet*/
      click_chatter("%{element} removing mapping seq %d\n",
		    this,
		    _mappings[0]._seq);
    _mappings.pop_front();
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
    int seq = random();

    int map_index = _mappings.size();
    _mappings.push_back(SeqProbMap());
    _mappings[map_index]._seq = seq;


    int bcast_index = _packets.size();
    _packets.push_back(Broadcast());
    _packets[bcast_index]._seq = seq;
    _packets[bcast_index]._originated = true;
    _packets[bcast_index]._p = p_in;
    _packets[bcast_index]._num_rx = 0;
    _packets[bcast_index]._num_tx = 0;
    _packets[bcast_index]._first_rx = now;
    _packets[bcast_index]._actual_first_rx = true;
    _packets[bcast_index]._sent = false;
    _packets[bcast_index]._scheduled = false;
    _packets[bcast_index].t = NULL;
    _packets[bcast_index]._to_send = now;
    _packets[bcast_index]._rx_from = _ip;
    _packets[bcast_index]._expected_rx = expected_rx(seq, _ip);
    _packets[bcast_index]._nweight = neighbor_weight(_ip);
    _packets[bcast_index]._slot = 0;
    _packets[bcast_index]._delay_ms = 0;
    forward(&_packets[bcast_index]);



  } else {
    _packets_rx++;

    click_ether *eh = (click_ether *) p_in->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
    
    uint32_t seq = pk->seq();
    
    int map_index = -1;
    for (int x = 0; x < _mappings.size(); x++) {
      if (_mappings[x]._seq == seq) {
	map_index = x;
	break;
      }
    }

    bool seen_before = false;
    if (map_index == - 1) {
      map_index = _mappings.size();
      _mappings.push_back(SeqProbMap());
      _mappings[map_index]._seq = seq;
    }


    int bcast_index = -1;
    for (int x = 0; x < _packets.size(); x++) {
      if (_packets[x]._seq == seq) {
	seen_before = true;
	if (!_packets[x]._sent) {
	  bcast_index = x;
	  break;
	}
      }
    }
    
    IPAddress src = pk->get_hop(pk->num_hops() - 1);
    if (bcast_index == -1) {
      /* haven't seen this packet before */
      bcast_index = _packets.size();
      _packets.push_back(Broadcast());
      _packets[bcast_index]._seq = seq;
      _packets[bcast_index]._originated = false;
      _packets[bcast_index]._p = p_in;
      _packets[bcast_index]._num_rx = 0;
      _packets[bcast_index]._num_tx = 0;
      _packets[bcast_index]._first_rx = now;
      _packets[bcast_index]._actual_first_rx = !seen_before;
      _packets[bcast_index]._sent = false;
      _packets[bcast_index]._scheduled = false;
      
      _packets[bcast_index].t = NULL;

      if (_debug) {
	click_chatter("%{element} first_rx seq %d src %s",
		      this,
		      _packets[bcast_index]._seq,
		      src.s().cc());
      }
      if (!seen_before) {
	/* clone the packet and push it out */
	Packet *p_out = p_in->clone();
	output(1).push(p_out);
      }
    } else {
      if (_debug) {
	click_chatter("%{element} extra_rx seq %d src %s",
		      this,
		      _packets[bcast_index]._seq,
		      src.s().cc());
      }
    }
    if (!_packets[bcast_index]._scheduled) {
      _packets[bcast_index]._rx_from = src;
    } else {
      _packets[bcast_index]._extra_rx.push_back(src);
    }
    _packets[bcast_index]._num_rx++;

    update_probs(_packets[bcast_index]._seq, src);
    schedule_bcast(&_packets[bcast_index]);
  }
  trim_packets();
}


int
FlashFlood::get_link_prob(IPAddress from, IPAddress to) 
{

  int metric = _link_table->get_hop_metric(from, to);
  if (!metric) {
    return 0;
  }
  int prob = 100 * 100 / metric;

  prob = min(prob, 100);

  if (_lossy) {
    return prob;
  }
  /* if sba, just bimodal links! */
  return (prob > _neighbor_threshold) ? 100 : 0;

}


void 
FlashFlood::update_probs(int seq, IPAddress src) {

  Vector<IPAddress> neighbors = _link_table->get_neighbors(src);
  if (!set_prob(seq, src, 100)) {
    click_chatter("%{element} error setting prob seq d src %s to %d\n",
		  this, 
		  seq,
		  src.s().cc(),
		  100);
  }
  for (int x = 0; x < neighbors.size(); x++) {
    if (neighbors[x] == _ip) {
      /* don't update my prob, it's one */
      continue;
    }
    /*
     * p(got packet ever) = (1 - p(never))
     *                    = (1 - (p(not before))*(p(not now)))
     *                    = (1 - (1 - p(before))*(1 - p(now)))
     *
     */
    int p_now = get_link_prob(src, neighbors[x]);
    int p_before = 0;
    if (!get_prob(seq, neighbors[x], &p_before)) {
      click_chatter("%{element} error getting prob seq d src %s\n",
		    this, 
		    seq,
		    neighbors[x].s().cc());
    }

    int p_ever = (100 - (((100 - p_before) * (100 - p_now))/100));
    if (p_ever > 100) {
      click_chatter("%{element} src %s neighbor %s p_ever %d p_before %d p_now %d\n",
		    this,
		    src.s().cc(),
		    neighbors[x].s().cc(),
		    p_ever,
		    p_before,
		    p_now);
      sr_assert(false);
    }
    if (!set_prob(seq, neighbors[x], p_ever)) {
      click_chatter("%{element} error setting prob seq d src %s to %d\n",
		    this, 
		    seq,
		    neighbors[x].s().cc(),
		    p_ever);
    }
  }
  
}

void
FlashFlood::schedule_bcast(Broadcast *bcast) {    

  if (bcast->_scheduled) {
    return;
  }
  Vector<IPAddress> neighbors = _link_table->get_neighbors(_ip);
  int my_expected_rx = expected_rx(bcast->_seq, _ip);
  int my_neighbor_weight = neighbor_weight(_ip);
  
  int delay_ms = 0;


  bcast->_expected_rx = my_expected_rx;
  bcast->_nweight = my_neighbor_weight;

  int max_neighbor_weight = 0;
  for (int x = 0; x< neighbors.size(); x++) {
    int nw = neighbor_weight(neighbors[x]);
    max_neighbor_weight = max(nw, max_neighbor_weight);
  }


  if (_pick_slots) {
    int slot_erx = 0;
    int slot_nweight = 0;
    
    /* 
     * now see what slot I should fit in
     */
    for (int x = 0; x < neighbors.size(); x++) {
      if (neighbors[x] == _ip) {
	/* don't update my prob, it's one */
	continue;
      }
      int neighbor_expected_rx = expected_rx(bcast->_seq, neighbors[x]);
      
      int neighbor_nweight = neighbor_weight(neighbors[x]);
      
      int p_ever;
      get_prob(bcast->_seq, neighbors[x], &p_ever);
      
      int metric = get_link_prob(_ip, neighbors[x]);
      
      int metric_from_sender = get_link_prob(bcast->_rx_from, neighbors[x]);
      if (_debug) {
	click_chatter("%{element} seq %d sender %s neighbor %s prob %d neighbor-weight %d expected_rx %d metric %d\n",
		      this,
		      bcast->_seq,
		      bcast->_rx_from.s().cc(),
		      neighbors[x].s().cc(),
		      p_ever,
		      neighbor_nweight,
		      neighbor_expected_rx,
		      metric);
      }
      if (neighbor_expected_rx > my_expected_rx && 
	   metric_from_sender > 1) {
	slot_erx++;
      } else if (my_neighbor_weight < neighbor_nweight ) {
	slot_nweight++;
      }
    }

    if (_slots_erx) {
      delay_ms = _slot_time_ms * slot_erx;
      bcast->_slot = slot_erx;
    } else if (_slots_nweight) {
      delay_ms = _slot_time_ms * slot_nweight;
      bcast->_slot = slot_nweight;
    }

    if (my_expected_rx > 0 && _debug) {
      click_chatter("%{element} seq %d sender %s erx_slot %d nweight_slot %d neighbor_weight %d max_neighbor_weight %d expected_rx %d\n",
		    this,
		    bcast->_seq,
		    bcast->_rx_from.s().cc(),
		    slot_erx,
		    slot_nweight,
		    my_neighbor_weight,
		    max_neighbor_weight,
		    my_expected_rx
		    );
    }



  } else {
    /* don't pick slots */
    /* pick based on relative neighbor weight */
    if (my_neighbor_weight) {
      int max_delay_ms = _slot_time_ms * max_neighbor_weight / my_neighbor_weight;
      delay_ms = random() % max_delay_ms;

      if (_debug) {
	click_chatter("%{element} seq %d sender %s max_delay_ms %d delay_ms %d expected_rx %d\n",
		      this,
		      bcast->_seq,
		      bcast->_rx_from.s().cc(),
		      max_delay_ms,
		      delay_ms,
		      my_expected_rx
		      );
      }
      
    }
  }


  /*
   * now, schedule a broadcast 
   *
   */
  if (_debug) {
    click_chatter("%{element} reschedule seq %d delay_ms %d\n",
		  this,
		  bcast->_seq,
		  delay_ms
		  );
  }
  
  delay_ms = max(delay_ms, 1);

  bcast->_delay_ms = delay_ms;

  sr_assert(delay_ms > 0);
  struct timeval now;
  click_gettimeofday(&now);
  struct timeval delay;
  delay.tv_sec = 0;
  delay.tv_usec = delay_ms*1000;
  timeradd(&now, &delay, &bcast->_to_send);
  bcast->del_timer();
  /* schedule timer */
  bcast->_scheduled = true;
  bcast->_sent = false;
  bcast->t = new Timer(static_forward_hook, (void *) this);
  bcast->t->initialize(this);
  bcast->t->schedule_at(bcast->_to_send);

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

  sa << "originated " << _packets_originated;
  sa << " tx " << _packets_tx;
  sa << " rx " << _packets_rx;
  sa << "\n";
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

int
FlashFlood::static_write_clear(const String &, Element *e,
			void *, ErrorHandler *) 
{
  FlashFlood *n = (FlashFlood *) e;
  n->clear();
  return 0;
}

void
FlashFlood::clear() {
  _mappings.clear();
  _packets.clear();
}
int
FlashFlood::static_write_min_p(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  FlashFlood *n = (FlashFlood *) e;
  int b;

  if (!cp_integer(arg, &b))
    return errh->error("`min_p' must be a integer");

  n->_min_p = b;
  return 0;
}

int
FlashFlood::static_write_threshold(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  FlashFlood *n = (FlashFlood *) e;
  int b;

  if (!cp_integer(arg, &b))
    return errh->error("`threshold' must be a integer");

  n->_threshold = b;
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
FlashFlood::static_print_min_p(Element *f, void *)
{
  StringAccum sa;
  FlashFlood *d = (FlashFlood *) f;
  sa << d->_min_p << "\n";
  return sa.take_string();
}

String
FlashFlood::static_print_threshold(Element *f, void *)
{
  StringAccum sa;
  FlashFlood *d = (FlashFlood *) f;
  sa << d->_threshold << "\n";
  return sa.take_string();
}

String
FlashFlood::print_packets()
{
  StringAccum sa;
  for (int x = 0; x < _packets.size(); x++) {
    sa << "ip " << _ip;
    sa << " seq " << _packets[x]._seq;
    sa << " originated " << _packets[x]._originated;
    sa << " actual_first_rx " << _packets[x]._actual_first_rx;
    sa << " num_rx " << _packets[x]._num_rx;
    sa << " num_tx " << _packets[x]._num_tx;
    sa << " delay " << _packets[x]._delay_ms;
    sa << " nweight " << _packets[x]._nweight;
    sa << " erx " << _packets[x]._expected_rx;
    sa << " slot " << _packets[x]._slot;
    sa << " rx_from " << _packets[x]._rx_from;
    
    sa << " ";
    for (int y = 0; y < _packets[x]._extra_rx.size(); y++) {
      sa << _packets[x]._extra_rx[y] << " ";
    }

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
  add_read_handler("min_p", static_print_min_p, 0);
  add_read_handler("threshold", static_print_threshold, 0);

  add_write_handler("debug", static_write_debug, 0);
  add_write_handler("min_p", static_write_min_p, 0);
  add_write_handler("clear", static_write_clear, 0);
  add_write_handler("threshold", static_write_threshold, 0);
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
