/*
 * CounterFlood.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachuscounterfloods Institute of Technology
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
#include "counterflood.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
CLICK_DECLS

CounterFlood::CounterFlood()
  :  _en(),
     _et(0),
     _packets_originated(0),
     _packets_tx(0),
     _packets_rx(0)
{

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

CounterFlood::~CounterFlood()
{
}

int
CounterFlood::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  _count = 1;
  _max_delay_ms = 750;
  _history = 100;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
                    "BCAST_IP", cpIPAddress, "IP address", &_bcast_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "COUNT", cpInteger, "Count", &_count,
		    "MAX_DELAY_MS", cpUnsigned, "Max Delay (ms)", &_max_delay_ms,
		    /* below not required */
		    "DEBUG", cpBool, "Debug", &_debug,
		    "HISTORY", cpUnsigned, "history", &_history,
                    cpEnd);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_bcast_ip) 
    return errh->error("BCAST_IP not specified");
  if (!_en) 
    return errh->error("ETH not specified");

  return ret;
}

int
CounterFlood::initialize (ErrorHandler *)
{
  return 0;
}

void
CounterFlood::forward(Broadcast *bcast) {

  if (_debug) {
    click_chatter("%{element} seq %d my_expected_rx %d sending\n",
		  this,
		  bcast->_seq
		  );
  }
  if (_debug) {
    click_chatter("%{element} forwarding seq %d\n",
		  this,
		  bcast->_seq);
  }
  Packet *p_in = bcast->_p;
  if (!p_in) {
    return;
  }
  click_ether *eh_in = (click_ether *) p_in->data();
  struct srpacket *pk_in = (struct srpacket *) (eh_in+1);

  int hops = 1;
  int len = 0;
  if (bcast->_originated) {
    hops = 1;
    len = srpacket::len_with_data(hops, p_in->length());
  } else {
    hops = pk_in->num_links() + 1;
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
  pk->set_num_links(hops);
  for (int x = 0; x < hops; x++) {
    pk->set_link_node(x, pk_in->get_link_node(x));
  }
  pk->set_link_node(hops,_ip);
  pk->set_next(hops);
  pk->set_seq(bcast->_seq);
  uint32_t link_seq = random();
  pk->set_seq2(link_seq);

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
  output(0).push(p);

  _packets_tx++;
  
  bcast->_actually_sent = true;
  bcast->_rx_from.push_back(_ip);
  bcast->_rx_from_seq.push_back(link_seq);

  

}

void
CounterFlood::forward_hook() 
{
  Timestamp now = Timestamp::now();
  for (int x = 0; x < _packets.size(); x++) {
    if (now > _packets[x]._to_send) {
      /* this timer has expired */
      if (!_packets[x]._forwarded && 
	  (!_count || _packets[x]._num_rx < _count)) {
	/* we haven't forwarded this packet yet */
	forward(&_packets[x]);
      }
      _packets[x]._forwarded = true;
    }
  }
}


void
CounterFlood::trim_packets() {
  /* only keep track of the last _max_packets */

  while ((_packets.size() > _history)) {
    /* unschedule and remove packet*/
    if (_debug) {
      click_chatter("%{element} removing seq %d\n",
		    this,
		    _packets[0]._seq);
    }
    _packets[0].del_timer();
    if (_packets[0]._p != 0) {
      _packets[0]._p->kill();
    }
    _packets.pop_front();
  }

}
void
CounterFlood::push(int port, Packet *p_in)
{
  Timestamp now = Timestamp::now();
  
  if (port == 1) {
    _packets_originated++;
    /* from me */
    int index = _packets.size();
    _packets.push_back(Broadcast());
    _packets[index]._seq = random();
    _packets[index]._originated = true;
    _packets[index]._p = p_in->clone();
    _packets[index]._num_rx = 0;
    _packets[index]._first_rx = now;
    _packets[index]._forwarded = true;
    _packets[index]._actually_sent = false;
    _packets[index].t = NULL;
    _packets[index]._to_send = now;
    if (_debug) {
      click_chatter("%{element} original packet %d, seq %d\n",
		    this,
		    _packets_originated,
		    _packets[index]._seq);
    }
    p_in->kill();
    forward(&_packets[index]);
  } else {
    _packets_rx++;

    click_ether *eh = (click_ether *) p_in->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
    
    uint32_t seq = pk->seq();
    uint32_t link_seq = pk->seq2();

    int index = -1;
    for (int x = 0; x < _packets.size(); x++) {
      if (_packets[x]._seq == seq) {
	index = x;
	break;
      }
    }

    IPAddress src = pk->get_link_node(pk->num_links() - 1);
    if (index == -1) {
      /* haven't seen this packet before */
      index = _packets.size();
      _packets.push_back(Broadcast());
      _packets[index]._seq = seq;
      _packets[index]._originated = false;
      _packets[index]._p = p_in->clone();
      _packets[index]._num_rx = 0;
      _packets[index]._first_rx = now;
      _packets[index]._forwarded = false;
      _packets[index]._actually_sent = false;
      _packets[index].t = NULL;
      _packets[index]._rx_from.push_back(src);
      _packets[index]._rx_from_seq.push_back(link_seq);

      /* schedule timer */
      int delay_time = (random() % _max_delay_ms) + 1;
      sr_assert(delay_time > 0);
      
      _packets[index]._to_send = now + Timestamp::make_msec(delay_time);
      _packets[index].t = new Timer(static_forward_hook, (void *) this);
      _packets[index].t->initialize(this);
      _packets[index].t->schedule_at(_packets[index]._to_send);

      if (_debug) {
	click_chatter("%{element} first_rx seq %d src %s",
		      this,
		      _packets[index]._seq,
		      src.s().c_str());
      }
      /* finally, clone the packet and push it out */
      output(1).push(p_in);
    } else {
      if (_debug) {
	click_chatter("%{element} extra_rx seq %d src %s",
		      this,
		      _packets[index]._seq,
		      src.s().c_str());
      }
      /* we've seen this packet before */
      p_in->kill();
    }
    _packets[index]._num_rx++;
    _packets[index]._rx_from.push_back(src);
    _packets[index]._rx_from_seq.push_back(link_seq);
      

  }

  trim_packets();
}


String
CounterFlood::print_packets()
{
  StringAccum sa;
  for (int x = 0; x < _packets.size(); x++) {
    sa << "ip " << _ip;
    sa << " seq " << _packets[x]._seq;
    sa << " originated " << _packets[x]._originated;
    sa << " actual_first_rx true";
    sa << " num_rx " << _packets[x]._num_rx;
    sa << " num_tx " << (int) _packets[x]._actually_sent;
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
CounterFlood::read_param(Element *e, void *vparam)
{
  CounterFlood *f = (CounterFlood *) e;
  switch ((int)vparam) {
  case 0:			
    return cp_unparse_bool(f->_debug) + "\n";
  case 1:			
    return String(f->_history) + "\n";
  case 2:			
    return String(f->_count) + "\n";
  case 3:			
    return String(f->_max_delay_ms) + "\n";
  case 4:
    return f->print_packets();
  default:
    return "";
  }
}
int 
CounterFlood::write_param(const String &in_s, Element *e, void *vparam,
			 ErrorHandler *errh)
{
  CounterFlood *f = (CounterFlood *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case 0: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }

  case 1: {			// history
    int history;
    if (!cp_integer(s, &history))
      return errh->error("history parameter must be integer");
    f->_history = history;
    break;
  }

  case 2: {			// count
    int count;
    if (!cp_integer(s, &count))
      return errh->error("count parameter must be integer");
    f->_count = count;
    break;
  }
  case 3: {			// max_delay_ms
    int max_delay_ms;
    if (!cp_integer(s, &max_delay_ms))
      return errh->error("max_delay_ms parameter must be integer");
    f->_max_delay_ms = max_delay_ms;
    break;
  }

  case 5: {	// clear
    f->_packets.clear();
    break;
  }
  }
  return 0;
}
void
CounterFlood::add_handlers()
{
  add_read_handler("debug", read_param, (void *) 0);
  add_read_handler("history", read_param, (void *) 1);
  add_read_handler("count", read_param, (void *) 2);
  add_read_handler("max_delay_ms", read_param, (void *) 3);
  add_read_handler("packets", read_param, (void *) 4);

  add_write_handler("debug", write_param, (void *) 0);
  add_write_handler("history", write_param, (void *) 1);
  add_write_handler("count", write_param, (void *) 2);
  add_write_handler("max_delay_ms", write_param , (void *) 3);
  add_write_handler("clear", write_param, (void *) 5);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<CounterFlood::Broadcast>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(CounterFlood)
