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
  :  Element(2,2),
     _en(),
     _et(0),
     _packets_originated(0),
     _packets_tx(0),
     _packets_rx(0)
{
  MOD_INC_USE_COUNT;

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

CounterFlood::~CounterFlood()
{
  MOD_DEC_USE_COUNT;
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
		    "MAX_DELAY", cpUnsigned, "Max Delay (ms)", &_max_delay_ms,
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

  return ret;
}

CounterFlood *
CounterFlood::clone () const
{
  return new CounterFlood;
}

int
CounterFlood::initialize (ErrorHandler *)
{
  return 0;
}

void
CounterFlood::forward(Broadcast *bcast) {

  if (_debug) {
    click_chatter("%{element} seq %d sender %s my_expected_rx %d sending\n",
		  this,
		  bcast->_seq,
		  bcast->_rx_from.s().cc(),
		  0);
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
  bcast->_actually_sent = true;
  _packets_tx++;
  
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);
  memset(eh->ether_dhost, 0xff, 6);
  output(0).push(p);

}

void
CounterFlood::forward_hook() 
{
  struct timeval now;
  click_gettimeofday(&now);
  for (int x = 0; x < _packets.size(); x++) {
    if (timercmp(&now, &_packets[x]._to_send, >)) {
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
  struct timeval now;
  click_gettimeofday(&now);
  
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
    _packets[index]._rx_from = _ip;
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
    
    int index = -1;
    for (int x = 0; x < _packets.size(); x++) {
      if (_packets[x]._seq == seq) {
	index = x;
	break;
      }
    }

    IPAddress src = pk->get_hop(pk->num_hops() - 1);
    if (index == -1) {
      /* haven't seen this packet before */
      index = _packets.size();
      _packets.push_back(Broadcast());
      _packets[index]._seq = seq;
      _packets[index]._originated = false;
      _packets[index]._p = p_in->clone();
      _packets[index]._num_rx = 1;
      _packets[index]._first_rx = now;
      _packets[index]._forwarded = false;
      _packets[index]._actually_sent = false;
      _packets[index].t = NULL;
      _packets[index]._rx_from = src;

      /* schedule timer */
      int delay_time = (random() % _max_delay_ms) + 1;
      sr_assert(delay_time > 0);
      
      struct timeval delay;
      delay.tv_sec = 0;
      delay.tv_usec = delay_time*1000;
      timeradd(&now, &delay, &_packets[index]._to_send);
      _packets[index].t = new Timer(static_forward_hook, (void *) this);
      _packets[index].t->initialize(this);
      _packets[index].t->schedule_at(_packets[index]._to_send);

      if (_debug) {
	click_chatter("%{element} first_rx seq %d src %s",
		      this,
		      _packets[index]._seq,
		      src.s().cc());
      }
      /* finally, clone the packet and push it out */
      output(1).push(p_in);
    } else {
      if (_debug) {
	click_chatter("%{element} extra_rx seq %d src %s",
		      this,
		      _packets[index]._seq,
		      src.s().cc());
      }
      /* we've seen this packet before */
      _packets[index]._extra_rx.push_back(src);
      p_in->kill();
      _packets[index]._num_rx++;
    }
  }

  trim_packets();
}


String
CounterFlood::static_print_stats(Element *f, void *)
{
  CounterFlood *d = (CounterFlood *) f;
  return d->print_stats();
}

String
CounterFlood::print_stats()
{
  StringAccum sa;

  sa << "originated " << _packets_originated;
  sa << " tx " << _packets_tx;
  sa << " rx " << _packets_rx;
  sa << "\n";
  return sa.take_string();
}

String
CounterFlood::static_print_count(Element *f, void *)
{
  CounterFlood *d = (CounterFlood *) f;
  return d->print_count();
}

String
CounterFlood::print_count()
{
  StringAccum sa;
  sa << _count << "\n";
  return sa.take_string();
}

int
CounterFlood::static_write_debug(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  CounterFlood *n = (CounterFlood *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`debug' must be a boolean");

  n->_debug = b;
  return 0;
}

int
CounterFlood::static_write_count(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  CounterFlood *n = (CounterFlood *) e;
  int b;

  if (!cp_integer(arg, &b))
    return errh->error("`count' must be a integer");

  n->_count = b;
  return 0;
}

int
CounterFlood::static_write_clear(const String &, Element *e,
			void *, ErrorHandler *) 
{
  CounterFlood *n = (CounterFlood *) e;
  n->clear();
  return 0;
}

void 
CounterFlood::clear() 
{
  _packets.clear();
}
String
CounterFlood::static_print_debug(Element *f, void *)
{
  StringAccum sa;
  CounterFlood *d = (CounterFlood *) f;
  sa << d->_debug << "\n";
  return sa.take_string();
}

String
CounterFlood::print_packets()
{
  StringAccum sa;
  for (int x = 0; x < _packets.size(); x++) {
    sa << "ip " << _ip;
    sa << " seq " << _packets[x]._seq;
    sa << " originated " << _packets[x]._originated;
    sa << " num_rx " << _packets[x]._num_rx;
    sa << " num_tx " << (int) _packets[x]._actually_sent;
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
CounterFlood::static_print_packets(Element *f, void *)
{
  CounterFlood *d = (CounterFlood *) f;
  return d->print_packets();
}

void
CounterFlood::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_read_handler("debug", static_print_debug, 0);
  add_read_handler("packets", static_print_packets, 0);
  add_read_handler("count", static_print_count, 0);

  add_write_handler("debug", static_write_debug, 0);
  add_write_handler("count", static_write_count, 0);
  add_write_handler("clear", static_write_clear, 0);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<CounterFlood::Broadcast>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(CounterFlood)
