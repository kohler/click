// -*- c-basic-offset: 4 -*-
/*
 * ecnqueue.{cc,hh} -- NotifierQueue with FIFO and LIFO inputs
 * John Bicket
 *
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/error.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/elemfilter.hh>
#include <elements/wifi/sr/path.hh>
#include <elements/standard/notifierqueue.hh>
#include <elements/wifi/sr/srforwarder.hh>
#include "srpacket.hh"
#include <click/router.hh>
#include <clicknet/tcp.h>
#include <click/ipflowid.hh>
#include "ecnqueue.hh"
CLICK_DECLS

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

ECNQueue::ECNQueue()
{
    set_ninputs(1);
    set_noutputs(1);
}

ECNQueue::~ECNQueue()
{
}

void *
ECNQueue::cast(const char *n)
{
    if (strcmp(n, "ECNQueue") == 0)
	return (ECNQueue *)this;
    else
	return NotifierQueue::cast(n);
}

int
ECNQueue::configure (Vector<String> &conf, ErrorHandler *errh)
{

    ActiveNotifier::initialize(router());
    
  int ret;
  _debug = false;
  int new_capacity = 1000;


  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "LENGTH", cpUnsigned, "maximum queue length", &new_capacity,
		    "DEBUG", cpBool, "Debug", &_debug,
                    cpEnd);
  if (ret < 0) {
    return ret;
  }

  _capacity = new_capacity;
  if (ret < 0) {
    return ret;
  }

  return 0;
}


ECNQueue::PathInfo *
ECNQueue::find_path_info(Path p) 
{

    PathInfo *nfo = _paths.findp(p);
    
    if (nfo) {
	return nfo;
    } 

    _paths.insert(p, PathInfo(p));
    return _paths.findp(p);
}


int 
ECNQueue::bubble_up(Packet *p_in)
{
    click_ether *eh = (click_ether *) p_in->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
    Path p = pk->get_path();
    bool reordered = false;
    for (int x = _head; x != _tail; x = next_i(x)) {
	click_ether *eh2 =  (click_ether *) _q[x]->data();
	struct srpacket *pk2 = (struct srpacket *) (eh2+1);
	Path p2 = pk2->get_path();
	if (p == p2) {
	    if (pk->data_seq() == pk2->data_seq()) {
		/* packet dup */
		struct timeval now;
		click_gettimeofday(&now);
		StringAccum sa;
		sa << id() << " " << now;
		sa << " dup! ";
		sa << " pk->seq " << pk->data_seq();
		sa << " on ";
		sa << path_to_string(p);
		click_chatter("%s", sa.take_string().cc());

		p_in->kill();
		return 0;
	    } else if (pk->data_seq() < pk2->data_seq()) {
		if (!reordered) {
		    reordered = true;
		    if (_debug) {
			struct timeval now;
			click_gettimeofday(&now);
			StringAccum sa;
			sa << id() << " " << now;
			sa << " reordering ";
			sa << " pk->seq " << pk->data_seq();
			sa << " pk2->seq " << pk2->data_seq();
			sa << " on ";
			sa << path_to_string(p);
			click_chatter("%s", sa.take_string().cc());
		    }
		}
		Packet *tmp = _q[x];
		_q[x] = p_in;
		p_in = tmp;
		p = p2;
	    }
	}

    }

    eh = (click_ether *) p_in->data();
    pk = (struct srpacket *) (eh+1);

    PathInfo *nfo = find_path_info(p);
    
    if (!enq(p_in)) {
	nfo->_ecn = true;
	struct timeval now;
	click_gettimeofday(&now);
	StringAccum sa;
	sa << "ECNQueue " << now;
	sa << " drop";
	sa << " pk->seq " << pk->data_seq();
	sa << path_to_string(nfo->_p);
	click_chatter("%s", sa.take_string().cc());
    } else if (nfo->_ecn) {
	pk->set_flag(FLAG_ECN);
	/* finally, we altered the packet, so we need to redo 
	 * the checksum
	 */
	pk->set_checksum();
	nfo->_ecn = false;
    }
    return 0;

}

void
ECNQueue::push(int, Packet *p_in)
{
    bubble_up(p_in);
    if (!signal_active() && !empty()) {
	wake_listeners();
    }
    return;
}

Packet *
ECNQueue::pull(int) {
    Packet *packet = deq();

    if (packet)
	_sleepiness = 0;
    else if (++_sleepiness == SLEEPINESS_TRIGGER)
	sleep_listeners();
    return packet;
}

String
ECNQueue::static_print_stats(Element *f, void *)
{
    ECNQueue *d = (ECNQueue *) f;
    return d->print_stats();
}

String
ECNQueue::print_stats()
{
  StringAccum sa;

  struct timeval now;
  click_gettimeofday(&now);
  for(PathIter iter = _paths.begin(); iter; iter++) {
      PathInfo f = iter.value();
      struct timeval age = f.last_tx_age();
      sa << path_to_string(f._p) << " seq " << f._seq << " age " << age << "\n";
  }

  return sa.take_string();
}
String
ECNQueue::static_print_debug(Element *f, void *)
{
  StringAccum sa;
  ECNQueue *d = (ECNQueue *) f;
  sa << d->_debug << "\n";
  return sa.take_string();
}


int
ECNQueue::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  ECNQueue *n = (ECNQueue *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}

void
ECNQueue::clear() 
{
    _paths.clear();
    struct timeval now;
    click_gettimeofday(&now);
}

int
ECNQueue::static_write_debug(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  ECNQueue *n = (ECNQueue *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`debug' must be a boolean");

  n->_debug = b;
  return 0;
}



void 
ECNQueue::add_handlers()
{
    add_write_handler("clear", static_clear, 0);
    add_read_handler("stats", static_print_stats, 0);
    add_write_handler("debug", static_write_debug, 0);
    add_read_handler("debug", static_print_debug, 0);
    
    NotifierQueue::add_handlers();
}
// generate template instances
#include <click/bighashmap.cc>

#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<Path, PathInfo>;
#endif

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(ECNQueue)
