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
     _timer(this), 
     _en(),
     _et(0),
     _packets_originated(0),
     _packets_tx(0),
     _packets_rx(0)
{
  MOD_INC_USE_COUNT;

  // Pick a starting sequence number that we have not used before.
  struct timeval tv;
  click_gettimeofday(&tv);
  _seq = tv.tv_usec;

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
  ret = cp_va_parse(conf, this, errh,
                    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
                    "IP", cpIPAddress, "IP address", &_ip,
                    "BCAST_IP", cpIPAddress, "IP address", &_bcast_ip,
		    "ETH", cpEtherAddress, "EtherAddress", &_en,
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
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
CounterFlood::run_timer ()
{
  _timer.schedule_after_ms(1000);
}

// Send a packet.
// fills in ethernet header
void
CounterFlood::send(WritablePacket *p)
{
  click_ether *eh = (click_ether *) p->data();

  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, _en.data(), 6);
  memset(eh->ether_dhost, 0xff, 6);
  _packets_tx++;
  output(0).push(p);
}

void
CounterFlood::push(int port, Packet *p_in)
{
  
  if (port == 1) {
    /* from me */
    int hops = 1;
    int extra = srpacket::len_wo_data(hops) + sizeof(click_ether);
    int payload_len = p_in->length();
    WritablePacket *p = p_in->push(extra);
    if(p == 0)
      return;

    click_ether *eh = (click_ether *) p->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);

    memset(pk, '\0', srpacket::len_wo_data(hops));
    pk->_version = _sr_version;
    pk->_type = PT_DATA;
    pk->_dlen = htons(payload_len);
    pk->_flags = 0;
    pk->_qdst = _bcast_ip;
    pk->_seq = htonl(++_seq);
    pk->set_num_hops(hops);
    pk->set_hop(0,_ip);
    pk->set_next(0);
    _packets_originated++;
    send(p);

    return;

  } else {
    _packets_rx++;
    output(1).push(p_in);
    return;
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
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Vector<CounterFlood::IPAddress>;
template class DEQueue<CounterFlood::Seen>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(CounterFlood)
