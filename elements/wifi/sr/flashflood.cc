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
  _process_own_sends = false;

  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
                    "BCAST_IP", cpIPAddress, "IP address", &_bcast_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "LT", cpElement, "Linktable", &_link_table,
		    /* below not required */
		    "MIN_P", cpInteger, "Min P", &_min_p,
		    "DEBUG", cpBool, "Debug", &_debug,
		    "HISTORY", cpUnsigned, "history", &_history,
		    "LOSSY", cpBool, "mist", &_lossy,
		    "THRESHOLD", cpInteger, "Threshold", &_threshold,
		    "NEIGHBOR_THRESHOLD", cpInteger, "Neighbor Threshold", &_neighbor_threshold,
		    "PICK_SLOTS", cpBool, "Wait time selection", &_pick_slots,
		    "SLOTS_NEIGHBOR_WEIGHT", cpBool, "use max neighbor weight for slots", &_slots_nweight,
		    "SLOTS_EXPECTED_RX", cpBool, "foo", &_slots_erx,
		    "SLOT_TIME_MS", cpInteger, "time (in ms) for a slot", &_slot_time_ms,
		    "PROCESS_OWN_SENDS", cpBool, "foo", &_process_own_sends,
                    cpEnd);

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
      click_chatter("%{element} seq %d my_expected_rx %d not_sending\n",
		    this,
		    bcast->_seq,
		    my_expected_rx);
    }

    bcast->_sent = true;
    bcast->_scheduled = false;
    bcast->del_timer();
    return;
  }
  if (_debug) {
    click_chatter("%{element} seq %d my_expected_rx %d sending\n",
		  this,
		  bcast->_seq,
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
    pk->set_hop_seq(x, pk_in->get_hop_seq(x));
  }

  pk->set_hop(hops - 1,_ip);
  pk->set_next(hops);
  pk->set_seq(bcast->_seq);
  uint32_t link_seq = random();
  pk->set_seq2(link_seq);
  pk->set_hop_seq(hops - 1, link_seq);
  bcast->_sent_seq.push_back(link_seq);

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


  bcast->_sent = true;
  bcast->_scheduled = false;
  bcast->_num_tx++;
  _packets_tx++;
  bcast->del_timer();


  if (_process_own_sends) {
    Packet *p_copy = p->clone();
    /* now let ourselves know we sent a packet */
    process_packet(p_copy); 
  } else {
    update_probs(bcast->_seq, link_seq, _ip);
  }
  output(0).push(p);
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
    start_flood(p_in);
  } else {
    process_packet(p_in);
  }
  trim_packets();

}

void 
FlashFlood::start_flood(Packet *p_in) 
{
  struct timeval now;
  click_gettimeofday(&now);
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
  forward(&_packets[bcast_index]);
  
}
void
FlashFlood::process_packet(Packet *p_in) 
{
    _packets_rx++;

    click_ether *eh = (click_ether *) p_in->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
    struct timeval now;
    uint32_t seq = pk->seq();
    uint32_t link_seq = pk->seq2();
    IPAddress src = pk->get_hop(pk->num_hops() - 1);

    click_gettimeofday(&now);

    int map_index = -1;
    for (int x = 0; x < _mappings.size(); x++) {
      if (_mappings[x]._seq == seq) {
	map_index = x;
	break;
      }
    }

    bool seen_seq_before = false;
    if (map_index == - 1) {
      map_index = _mappings.size();
      _mappings.push_back(SeqProbMap());
      _mappings[map_index]._seq = seq;
    }


    int bcast_index = -1;
    for (int x = 0; x < _packets.size(); x++) {
      if (_packets[x]._seq == seq) {
	seen_seq_before = true;
	if (_packets[x]._scheduled) {
	  bcast_index = x;
	  break;
	}
      }
    }

    for (int x = 0; x < pk->num_hops(); x++) {
      IPAddress prev_src = pk->get_hop(x);
      uint32_t prev_link_seq = pk->get_hop_seq(x);
      update_probs(seq, prev_link_seq, prev_src);
    }
    update_probs(seq, link_seq, src);


    if (bcast_index == -1) {
      if (!seen_seq_before) {
	/* clone the packet and push it out */
	Packet *p_out = p_in->clone();
	output(1).push(p_out);
      }


      /* start a new timer */
      bcast_index = _packets.size();
      _packets.push_back(Broadcast());
      _packets[bcast_index]._seq = seq;
      _packets[bcast_index]._originated = false;
      _packets[bcast_index]._p = p_in;
      _packets[bcast_index]._num_rx = 0;
      _packets[bcast_index]._num_tx = 0;
      _packets[bcast_index]._first_rx = now;
      _packets[bcast_index]._actual_first_rx = !seen_seq_before;
      _packets[bcast_index]._sent = false;
      _packets[bcast_index]._scheduled = true;
      _packets[bcast_index].t = new Timer(static_forward_hook, (void *) this);
      _packets[bcast_index].t->initialize(this);
      
      int delay_ms = max(get_wait_time(seq, src), 1);
      struct timeval delay;
      delay.tv_sec = 0;
      delay.tv_usec = delay_ms*1000;
      timeradd(&now, &delay, &_packets[bcast_index]._to_send);
      
      /* schedule timer */
      _packets[bcast_index].t->schedule_at(_packets[bcast_index]._to_send);
    } else {
      p_in->kill();
    }
    

    _packets[bcast_index]._num_rx++;
    _packets[bcast_index]._rx_from.push_back(src);
    _packets[bcast_index]._rx_from_seq.push_back(link_seq);
    
}


int 
FlashFlood::get_wait_time(uint32_t seq, IPAddress last_sender) {
  Vector<IPAddress> my_neighbors = _link_table->get_neighbors(_ip);
  int my_expected_rx = expected_rx(seq, _ip);
  int my_neighbor_weight = neighbor_weight(_ip);
  
  int delay_ms = 0;
  
  int max_neighbor_weight = 0;
  for (int x = 0; x< my_neighbors.size(); x++) {
    int nw = neighbor_weight(my_neighbors[x]);
    max_neighbor_weight = max(nw, max_neighbor_weight);
  }
  
  
  if (_pick_slots) {
    int slot_erx = 0;
    int slot_nweight = 0;
    
    /* 
     * now see what slot I should fit in
     */
    for (int x = 0; x < my_neighbors.size(); x++) {
      if (my_neighbors[x] == _ip) {
	/* don't update my prob, it's one */
	continue;
      }
      int neighbor_expected_rx = expected_rx(seq, my_neighbors[x]);
      
      int neighbor_nweight = neighbor_weight(my_neighbors[x]);
      
      int p_ever;
      get_prob(seq, my_neighbors[x], &p_ever);
      
      int metric = get_link_prob(_ip, my_neighbors[x]);
      

      int metric_from_sender = get_link_prob(last_sender, my_neighbors[x]);
      if (_debug) {
	click_chatter("%{element} seq %d sender %s neighbor %s prob %d neighbor-weight %d expected_rx %d metric %d\n",
		      this,
		      seq,
		      last_sender.s().cc(),
		      my_neighbors[x].s().cc(),
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
    } else if (_slots_nweight) {
      delay_ms = _slot_time_ms * slot_nweight;
    }

    if (my_expected_rx > 0 && _debug) {
      click_chatter("%{element} seq %d sender %s erx_slot %d nweight_slot %d neighbor_weight %d max_neighbor_weight %d expected_rx %d\n",
		    this,
		    seq,
		    last_sender.s().cc(),
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
		      seq,
		      last_sender.s().cc(),
		      max_delay_ms,
		      delay_ms,
		      my_expected_rx
		      );
      }
      
    }
  }

  return delay_ms;
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
FlashFlood::update_probs(uint32_t seq, uint32_t link_seq, IPAddress src) {

  if (!link_seq) {
    click_chatter("%{element} link_seq is 0 for seq %d src %s\n",
		  this, 
		  seq,
		  src.s().cc());
  }
  SeqProbMap *m = findmap(seq);
  if (!m) {
    click_chatter("%{element} error fetching map for seq  %dn",
		  this, 
		  seq);
    return;
  }
  int index =  -1;
  for (int x = 0; x < m->_senders.size(); x++) {
    if (m->_senders[x] == src && m->_link_seq[x] == link_seq) {
      index = x;
      break;
    }
  }

  if (index == -1) {
    m->_senders.push_back(src);
    m->_link_seq.push_back(link_seq);
  } else {
    /* already updated this broadcast */
    return;
  }


  Vector<IPAddress> neighbors = _link_table->get_neighbors(src);
  if (!set_prob(seq, src, 100)) {
    click_chatter("%{element} error setting prob seq %d src %s to %d\n",
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
    sa << " sent_seqs ";
    for (int y = 0; y < _packets[x]._sent_seq.size(); y++) {
      sa << _packets[x]._sent_seq[y] << " ";
    }
    sa << " rx_from "; 

    for (int y = 0; y < _packets[x]._rx_from.size(); y++) {
      sa << _packets[x]._rx_from[y] << " " << _packets[x]._rx_from_seq[y] << " ";
    }

    sa << "\n";
  }
  return sa.take_string();
}

String
FlashFlood::read_param(Element *e, void *vparam)
{
  FlashFlood *f = (FlashFlood *) e;
  switch ((int)vparam) {
  case 0:			
    return cp_unparse_bool(f->_debug) + "\n";
  case 1:			
    return String(f->_min_p) + "\n";
  case 2:			
    return String(f->_history) + "\n";
  case 3:			
    return String(f->_lossy) + "\n";
  case 4:			
    return String(f->_threshold) + "\n";
  case 5:			
    return String(f->_neighbor_threshold) + "\n";
  case 6:			
    return String(f->_pick_slots) + "\n";
  case 7:			
    return String(f->_slots_nweight) + "\n";
  case 8:			
    return String(f->_slots_erx) + "\n";
  case 9:			
    return String(f->_slot_time_ms) + "\n";
  case 10:
    return f->print_packets();
  case 12:
    return String(f->_process_own_sends) + "\n";;
  default:
    return "";
  }
}

int 
FlashFlood::write_param(const String &in_s, Element *e, void *vparam,
			 ErrorHandler *errh)
{
  FlashFlood *f = (FlashFlood *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case 0: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case 1: {	// min_p
    int min_p;
    if (!cp_integer(s, &min_p))
      return errh->error("min_p parameter must be integer");
    f->_min_p = min_p;
    break;
  }
  case 2: {			// history
    int history;
    if (!cp_integer(s, &history))
      return errh->error("history parameter must be integer");
    f->_history = history;
    break;
  }
  case 3: {    //lossy
    bool lossy;
    if (!cp_bool(s, &lossy)) 
      return errh->error("lossy parameter must be boolean");
    f->_lossy = lossy;
    break;
  }
     case 4: {			// threshold
    int threshold;
    if (!cp_integer(s, &threshold))
      return errh->error("threshold parameter must be integer");
    f->_threshold = threshold;
    break;
  } 
     case 5: {			// neighbor_threshold
    int neighbor_threshold;
    if (!cp_integer(s, &neighbor_threshold))
      return errh->error("neighbor_threshold parameter must be integer");
    f->_neighbor_threshold = neighbor_threshold;
    break;
  }
  case 6: {    //pick_slots
    bool pick_slots;
    if (!cp_bool(s, &pick_slots)) 
      return errh->error("pick_slots parameter must be boolean");
    f->_pick_slots = pick_slots;
    break;
  } 

  case 7: {    //slots_neighbor_weight
    bool slots_neighbor_weight;
    if (!cp_bool(s, &slots_neighbor_weight)) 
      return errh->error("slots_neighbor_weight parameter must be boolean");
    f->_slots_nweight = slots_neighbor_weight;
    break;
  } 

  case 8: {    //slots_expected_rx
    bool slots_expected_rx;
    if (!cp_bool(s, &slots_expected_rx)) 
      return errh->error("slots_expected_rx parameter must be boolean");
    f->_slots_erx = slots_expected_rx;
    break;
  }

  case 9: {	// slot_time_ms
    int slot_time_ms;
    if (!cp_integer(s, &slot_time_ms))
      return errh->error("slot_time_ms parameter must be integer");
    f->_slot_time_ms = slot_time_ms;
    break;
  }

  case 11: {	// clear
    f->_packets.clear();
    f->_mappings.clear();
    break;
  }

  case 12: {    //process_own_sends
    bool process_own_sends;
    if (!cp_bool(s, &process_own_sends)) 
      return errh->error("process_own_sends parameter must be boolean");
    f->_slots_erx = process_own_sends;
    break;
  }
  }
  return 0;
}


void
FlashFlood::add_handlers()
{
  add_read_handler("debug", read_param, (void *) 0);
  add_read_handler("min_p", read_param, (void *) 1);
  add_read_handler("history", read_param,(void *) 2);
  add_read_handler("lossy", read_param, (void *) 3);
  add_read_handler("threshold", read_param, (void *) 4);
  add_read_handler("neighbor_threshold", read_param, (void *) 5);
  add_read_handler("pick_slots", read_param, (void *) 6);
  add_read_handler("slots_neighbor_weight", read_param, (void *) 7);
  add_read_handler("slots_expected_rx", read_param, (void *) 8);
  add_read_handler("slot_time_ms", read_param, (void *) 9);
  add_read_handler("packets", read_param, (void *) 10);
  add_read_handler("process_own_sends", read_param, (void *) 12);
  
  add_write_handler("debug", write_param, (void *) 0);
  add_write_handler("min_p", write_param, (void *) 1);
  add_write_handler("history", write_param,(void *) 2);
  add_write_handler("lossy", write_param, (void *) 3);
  add_write_handler("threshold", write_param, (void *) 4);
  add_write_handler("neighbor_threshold", write_param, (void *) 5);
  add_write_handler("pick_slots", write_param, (void *) 6);
  add_write_handler("slots_neighbor_weight", write_param, (void *) 7);
  add_write_handler("slots_expected_rx", write_param, (void *) 8);
  add_write_handler("slot_time_ms", write_param, (void *) 9);
  add_write_handler("clear", write_param, (void *) 11);
  add_write_handler("process_own_sends", write_param, (void *) 12);

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
