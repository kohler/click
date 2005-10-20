/*
 * PFlood.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachuspfloods Institute of Technology
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
#include "pflood.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "srpacket.hh"
CLICK_DECLS

PFlood::PFlood()
  :  _en(),
     _et(0),
     _packets_originated(0),
     _packets_tx(0),
     _packets_rx(0)
{

  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

PFlood::~PFlood()
{
}

int
PFlood::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  _p = 0;
  _max_delay_ms = 750;
  _history = 100;
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
                    "BCAST_IP", cpIPAddress, "IP address", &_bcast_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
		    "P", cpInteger, "Count", &_p,
		    "MAX_DELAY", cpUnsigned, "Max Delay (ms)", &_max_delay_ms,
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
  if (_p < 0 || _p > 100)
    return errh->error ("P must be between 0 and 100");


  return ret;
}

int
PFlood::initialize (ErrorHandler *)
{
  return 0;
}

void
PFlood::forward(Broadcast *bcast) {
  Packet *p_in = bcast->_p;
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
  pk->unset_flag(~0);
  pk->set_qdst(_bcast_ip);
  pk->set_num_links(hops);
  for (int x = 0; x < hops - 1; x++) {
    pk->set_link_node(x, pk_in->get_link_node(x));
  }
  pk->set_link_node(hops - 1,_ip);
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
PFlood::forward_hook() 
{
  Timestamp now = Timestamp::now();
  for (int x = 0; x < _packets.size(); x++) {
    if (_packets[x]._to_send <= now) {
      /* this timer has expired */
      if (!_packets[x]._forwarded) {
	/* we haven't forwarded this packet yet */
	if (random() % 100 <= _p) {
	  forward(&_packets[x]);
	} 
	_packets[x]._forwarded = true;
      }
    }
  }
}


void
PFlood::trim_packets() {
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
PFlood::push(int port, Packet *p_in)
{
    Timestamp now = Timestamp::now();
  
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
	_packets[index]._forwarded = true;
	_packets[index]._actually_sent = false;
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
      _packets[index]._num_rx = 1;
      _packets[index]._first_rx = now;
      _packets[index]._forwarded = false;
      _packets[index]._actually_sent = false;
      _packets[index].t = NULL;

      /* schedule timer */
      int delay_time = (random() % _max_delay_ms) + 1;
      sr_assert(delay_time > 0);
      
      _packets[index].t = new Timer(static_forward_hook, (void *) this);
      _packets[index].t->initialize(this);
      _packets[index].t->schedule_at(now + Timestamp::make_msec(delay_time));

      /* finally, clone the packet and push it out */
      Packet *p_out = p_in->clone();
      output(1).push(p_out);
    } else {
      /* we've seen this packet before */
      p_in->kill();
      _packets[index]._num_rx++;
    }
  }

  trim_packets();
}


String
PFlood::static_print_stats(Element *f, void *)
{
  PFlood *d = (PFlood *) f;
  return d->print_stats();
}

String
PFlood::print_stats()
{
  StringAccum sa;

  sa << "originated " << _packets_originated;
  sa << " tx " << _packets_tx;
  sa << " rx " << _packets_rx;
  sa << "\n";
  return sa.take_string();
}

int
PFlood::static_write_debug(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  PFlood *n = (PFlood *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`debug' must be a boolean");

  n->_debug = b;
  return 0;
}


int
PFlood::static_write_p(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  PFlood *n = (PFlood *) e;
  int b;

  if (!cp_integer(arg, &b))
    return errh->error("`p' must be an integer");

  n->_p = b;
  return 0;
}

String
PFlood::static_print_debug(Element *f, void *)
{
  StringAccum sa;
  PFlood *d = (PFlood *) f;
  sa << d->_debug << "\n";
  return sa.take_string();
}

String
PFlood::static_print_p(Element *f, void *)
{
  StringAccum sa;
  PFlood *d = (PFlood *) f;
  sa << d->_p << "\n";
  return sa.take_string();
}

String
PFlood::print_packets()
{
  StringAccum sa;
  for (int x = 0; x < _packets.size(); x++) {
    sa << "seq " << _packets[x]._seq;
    sa << " originated " << _packets[x]._originated;
    sa << " num_rx " << _packets[x]._num_rx;
    sa << " first_rx " << _packets[x]._first_rx;
    sa << " forwarded " << _packets[x]._forwarded;
    sa << " actually_sent " << _packets[x]._actually_sent;
    sa << " to_send " << _packets[x]._to_send;
    sa << "\n";
  }
  return sa.take_string();
}
String
PFlood::static_print_packets(Element *f, void *)
{
  PFlood *d = (PFlood *) f;
  return d->print_packets();
}

void
PFlood::add_handlers()
{
  add_read_handler("stats", static_print_stats, 0);
  add_read_handler("debug", static_print_debug, 0);
  add_read_handler("packets", static_print_packets, 0);
  add_read_handler("p", static_print_p, 0);

  add_write_handler("debug", static_write_debug, 0);
  add_write_handler("p", static_write_p, 0);
}

// generate Vector template instance
#include <click/vector.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<PFlood::Broadcast>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(PFlood)
