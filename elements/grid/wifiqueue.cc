/*
 * wifiqueue.{cc,hh} -- queue element
 * John Bicket
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "wifiqueue.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <click/straccum.hh>
#include "wifitxfeedback.hh"
CLICK_DECLS


#ifndef wifiqueue_assert
#define wifiqueue_assert(e) ((e) ? (void) 0 : wifiqueue_assert_(__FILE__, __LINE__, #e))
#endif /* wifiqueue_assert */

WifiQueue::WifiQueue()
{
  add_input();
  add_input();
  add_output();
  current_q = 0;
  MOD_INC_USE_COUNT;
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
  _capacity = 100;
  // click_chatter("WifiQueue: finished initializing\n");
}

WifiQueue::~WifiQueue()
{
  MOD_DEC_USE_COUNT;
}

void *
WifiQueue::cast(const char *n)
{
  if (strcmp(n, "WifiQueue") == 0)
    return (Element *)this;
  else
    return 0;
}

int
WifiQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
		    cpOptional,
		    0);

  return res;

}

int
WifiQueue::initialize(ErrorHandler *)
{

  return 0;
}

void
WifiQueue::cleanup(CleanupStage)
{
}


WifiQueue::Neighbor *
WifiQueue::find_neighbor(EtherAddress e) {
  Neighbor **n = _neighbors_m.findp(e);
  if (!n || !*n) {
    Neighbor *foo = new Neighbor(e);
    _neighbors_m.insert(e, foo);
    _neighbors_v.push_back(e);
    n = _neighbors_m.findp(e);
  }
  if (!n || !*n) {
    click_chatter("WifiQueue: no q found for %s\n", e.s().cc());
  }
  return (*n);
}
void 
WifiQueue::enq(Packet *p) 
{
  click_ether *eh = (click_ether *) p->data();
  if (!eh || !eh->ether_dhost) {
    return;
  }
  EtherAddress dst = EtherAddress(eh->ether_dhost);
  Neighbor *n = find_neighbor(dst);
  wifiqueue_assert(n);
  n->_q.enq(p);
  //_q.enq(p);

}

Packet *
WifiQueue::next() 
{

  if (_neighbors_v.size() == 0) {
    return (0);
  }

  current_q++;

  if (current_q >= _neighbors_v.size()) {
    current_q = 0;
  }

  Packet *p = 0;
  for(int i = 0; i < _neighbors_v.size(); i++) {
    Neighbor *n = find_neighbor(_neighbors_v[current_q]);
    wifiqueue_assert(n);
    p = n->_retry_q.deq();
    if (!p) {
      p = n->_q.deq();
    }
    if (p) {
      return p;
    }
    current_q++;
    if (current_q >= _neighbors_v.size()) {
      current_q = 0;
    }
  }
  //click_chatter("no packet to send\n");
  return (0);
}
void 
WifiQueue::feedback(Packet *p) 
{
  click_ether *eh = (click_ether *) p->data();
  
  EtherAddress dst = EtherAddress(eh->ether_dhost);
  
  Neighbor *n = find_neighbor(dst);
  if (!n) {
    click_chatter("WifiQueue couldn't find neighbor info for %s\n", 
		  dst.s().cc());
    p->kill();
    return;
  }

  int txcount = 1 + p->user_anno_c (TX_ANNO_SHORT_RETRIES);
  int success = p->user_anno_c (TX_ANNO_SUCCESS);
  int rate = p->user_anno_c (TX_ANNO_RATE);
  click_chatter("rate was %d, txcount %d for dst %s\n", rate, txcount, dst.s().cc());
  int metric = (rate * 50) / txcount;
  if (!n->metric) {
    n->metric = metric;
  } else {
    n->metric = (metric + 9*n->metric)/10;
  }

  if (success) {
    p->kill();
  } else {
    n->_retry_q.enq(p);
  }
  
}
void
WifiQueue::push(int port, Packet *p)
{

  switch (port) {
  case 0:
    enq(p);
    return;
  case 1:
    feedback(p);
    return;
  default:
    click_chatter("WifiQueue: bad port %d", port);
    return;
  }

}

Packet *
WifiQueue::pull(int)
{
  return next();
}


String
WifiQueue::static_print_stats(Element *e, void *)
{
  WifiQueue *n = (WifiQueue *) e;
  return n->print_stats();
}
String 
WifiQueue::print_stats() 
{
  StringAccum sa;
  for(NTIter iter = _neighbors_m.begin(); iter; iter++) {
    Neighbor *n = iter.value();
    sa << n->_eth.s().cc() <<"\n";
    sa << " metric:" << n->metric << "\n";
    sa << " q len :" << n->_q.size() << "\n";
    sa << "\n";
  }
  return sa.take_string();
}

void
WifiQueue::add_handlers() {
  add_read_handler("stats", static_print_stats, 0);

}


 void
WifiQueue::wifiqueue_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("WifiQueue assertion \"%s\" failed: file %s, line %d",
		expr, file, line);
#ifdef CLICK_USERLEVEL  
  abort();
#else
  click_chatter("Continuing execution anyway, hold on to your hats!\n");
#endif

}
#include <click/bighashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class BigHashMap<EtherAddress, WifiNeighbor *>;
template class Vector<WifiNeighbor *>;
#endif
CLICK_ENDDECLS
ELEMENT_PROVIDES(Storage)
EXPORT_ELEMENT(WifiQueue)
