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
  Packet *p_in = bcast->_p;
  click_ether *eh_in = (click_ether *) p_in->data();
  struct srpacket *pk_in = (struct srpacket *) (eh_in+1);

  int hops = 1;
  if (!bcast->_originated) {
    hops = pk_in->num_hops() + 1;
  }
  
  int len = srpacket::len_wo_data(hops);
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
  bcast->_forwarded = true;

  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);
  memset(eh->ether_dhost, 0xff, 6);
  _packets_tx++;
  output(0).push(p);

}

void
CounterFlood::forward_hook() 
{
  struct timeval now;
  click_gettimeofday(&now);
  for (int x = 0; x < _packets.size(); x++) {
    if (timercmp(&_packets[x]._to_send, &now, <) && !_packets[x]._forwarded) {
      forward(&_packets[x]);
    }
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
    _packets[index]._p = p_in;
    _packets[index]._num_rx = 0;
    _packets[index]._first_rx = now;
    _packets[index]._forwarded = false;
    _packets[index].t = NULL;
    _packets[index]._to_send = now;
    forward(&_packets[index]);
    return;

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
      _packets[index]._first_rx = now;
      _packets[index]._forwarded = false;
      _packets[index].t = NULL;

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
    } else {
      /* we've seen this packet before */
      p_in->kill();
      _packets[index]._num_rx++;
      if (_packets[index]._num_rx > _count) {
	/* unschedule */
	_packets[index].del_timer();
      }
    }
  }
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

  sa << "originated " << _packets_originated << "\n";
  sa << "tx " << _packets_tx << "\n";
  sa << "rx " << _packets_rx << "\n";
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
String
CounterFlood::static_print_debug(Element *f, void *)
{
  StringAccum sa;
  CounterFlood *d = (CounterFlood *) f;
  sa << d->_debug << "\n";
  return sa.take_string();
}
void
CounterFlood::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_read_handler("debug", static_print_debug, 0);

  add_write_handler("debug", static_write_debug, 0);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<CounterFlood::Broadcast>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(CounterFlood)
