// -*- c-basic-offset: 4 -*-
/*
 * inorderqueue.{cc,hh} -- NotifierQueue with FIFO and LIFO inputs
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
#include "inorderqueue.hh"
CLICK_DECLS

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

InOrderQueue::InOrderQueue()
    : _timer(this)
{
    set_ninputs(1);
    set_noutputs(1);
    MOD_INC_USE_COUNT;
}

InOrderQueue::~InOrderQueue()
{
    MOD_DEC_USE_COUNT;
}

void *
InOrderQueue::cast(const char *n)
{
    if (strcmp(n, "InOrderQueue") == 0)
	return (InOrderQueue *)this;
    else
	return NotifierQueue::cast(n);
}

int
InOrderQueue::configure (Vector<String> &conf, ErrorHandler *errh)
{

    ActiveNotifier::initialize(router());
    
  int ret;
  _debug = false;
  int packet_to = 0;
  int new_capacity = 1000;
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "LENGTH", cpUnsigned, "maximum queue length", &new_capacity,
		    "PACKET_TIMEOUT", cpUnsigned, "packet timeout", &packet_to,
		    "DEBUG", cpBool, "Debug", &_debug,
                    0);
  if (ret < 0) {
    return ret;
  }

  _capacity = new_capacity;
  ret = set_packet_timeout(errh, packet_to);
  if (ret < 0) {
    return ret;
  }

  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}
void
InOrderQueue::run_timer() 
{
    shove_out();

    _timer.schedule_after_ms(_max_tx_packet_ms/2);
}


InOrderQueue::PathInfo *
InOrderQueue::find_path_info(Path p) 
{

    PathInfo *nfo = _paths.findp(p);
    
    if (nfo) {
	return nfo;
    } 

    _paths.insert(p, PathInfo(p));
    return _paths.findp(p);
}
bool
InOrderQueue::ready_for(const Packet *p_in) {

    click_ether *eh = (click_ether *) p_in->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
    Path p = pk->get_path();

    PathInfo *nfo = _paths.findp(p);
    if (!nfo) {
	click_chatter("%{element} nfo for path is NULL! [%s]\n",
		      this,
		      path_to_string(p).cc());
	return true;
    }
    uint32_t seq = pk->data_seq();

    if (seq == nfo->_seq + 1) {
	/* this is the normal case */
	nfo->_seq = max(nfo->_seq, seq);
	click_gettimeofday(&nfo->_last_tx);
	return true;
    }

    if (!seq || !nfo->_seq) {
	/* reset on either end*/
	if (_debug && nfo->_seq) {
	    struct timeval now;
	    click_gettimeofday(&now);
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " reset [" << path_to_string(nfo->_p) << "]";
	    sa << " nfo_seq " << nfo->_seq;
	    sa << " pk_seq " << seq;
	    click_chatter("%s", sa.take_string().cc());
	}
	nfo->_seq = seq;
	click_gettimeofday(&nfo->_last_tx);
	return true;
    }
    
    if (seq <= nfo->_seq) {
	if (_debug) {
	    struct timeval now;
	    click_gettimeofday(&now);
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " <= [" << path_to_string(nfo->_p) << "]";
	    sa << " nfo_seq " << nfo->_seq;
	    sa << " pk_seq " << seq;
	    click_chatter("%s", sa.take_string().cc());
	}
	return true;
    }

    if (pk->flag(FLAG_ECN)) {
	if (_debug) {
	    struct timeval now;
	    click_gettimeofday(&now);
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " ecn [" << path_to_string(nfo->_p) << "]";
	    sa << " nfo_seq " << nfo->_seq;
	    sa << " pk_seq " << seq;
	    click_chatter("%s", sa.take_string().cc());
	}
	nfo->_seq = max(nfo->_seq, seq);
	click_gettimeofday(&nfo->_last_tx);
	return true;
    }

    struct timeval age = nfo->last_tx_age();
    if (timercmp(&age, &_packet_timeout, >)) {
	if (_debug) {
	    struct timeval now;
	    click_gettimeofday(&now);
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " timeout [" << path_to_string(nfo->_p) << "]";
	    sa << " nfo_seq " << nfo->_seq;
	    sa << " pk_seq " << seq;
	    sa << " " << age;
	    click_chatter("%s", sa.take_string().cc());
	}
	nfo->_seq = seq;
	click_gettimeofday(&nfo->_last_tx);
	return true;
    }

    return false;
}

int 
InOrderQueue::bubble_up(Packet *p_in)
{
    click_ether *eh = (click_ether *) p_in->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
    Path p = pk->get_path();
    bool reordered = false;
    PathInfo *nfo = find_path_info(p);
    
    if (pk->flag(FLAG_ECN)) {
	nfo->_seq = max(nfo->_seq, pk->data_seq());
    }

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
    uint32_t seq = pk->data_seq();
    if (!enq(p_in)) {
	nfo->_seq = max(seq, nfo->_seq);
	struct timeval now;
	click_gettimeofday(&now);
	StringAccum sa;
	sa << "InOrderQueue " << now;
	sa << " drop";
	sa << " pk->seq " << pk->data_seq();
	sa << path_to_string(nfo->_p);
	click_chatter("%s", sa.take_string().cc());
    }
    return 0;

}

void
InOrderQueue::push(int, Packet *p_in)
{
    bubble_up(p_in);
    shove_out();
    return;
}

void 
InOrderQueue::shove_out() {
    Packet *packet = NULL;
    do {
	if (packet) {
	    output(0).push(packet);
	}
	packet = yank1(yank_filter(this));
    } while (packet);


}

String
InOrderQueue::static_print_stats(Element *f, void *)
{
    InOrderQueue *d = (InOrderQueue *) f;
    return d->print_stats();
}

String
InOrderQueue::print_stats()
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
InOrderQueue::static_print_debug(Element *f, void *)
{
  StringAccum sa;
  InOrderQueue *d = (InOrderQueue *) f;
  sa << d->_debug << "\n";
  return sa.take_string();
}

String
InOrderQueue::static_print_packet_timeout(Element *f, void *)
{
  StringAccum sa;
  InOrderQueue *d = (InOrderQueue *) f;
  sa << d->_max_tx_packet_ms << "\n";
  return sa.take_string();
}

int
InOrderQueue::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  InOrderQueue *n = (InOrderQueue *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}

void
InOrderQueue::clear() 
{
    _paths.clear();
    struct timeval now;
    click_gettimeofday(&now);
}

int
InOrderQueue::static_write_debug(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  InOrderQueue *n = (InOrderQueue *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`debug' must be a boolean");

  n->_debug = b;
  return 0;
}


int
InOrderQueue::static_write_packet_timeout(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  InOrderQueue *n = (InOrderQueue *) e;
  unsigned int b;

  if (!cp_unsigned(arg, &b))
    return errh->error("`packet_timeout' must be a unsigned int");

  return n->set_packet_timeout(errh, b);
}

int
InOrderQueue::set_packet_timeout(ErrorHandler *errh, unsigned int x) 
{

  if (!x) {
    return errh->error("PACKET_TIMEOUT must not be 0");
  }
  _max_tx_packet_ms = x;
  _packet_timeout.tv_sec = x/1000;
  _packet_timeout.tv_usec = (x % 1000) * 1000;
  return 0;
}




void 
InOrderQueue::add_handlers()
{
    add_write_handler("clear", static_clear, 0);
    add_read_handler("stats", static_print_stats, 0);
    add_write_handler("debug", static_write_debug, 0);
    add_read_handler("debug", static_print_debug, 0);
    
    add_write_handler("packet_timeout", static_write_packet_timeout, 0);
    add_read_handler("packet_timeout", static_print_packet_timeout, 0);
    
    NotifierQueue::add_handlers();
}
// generate template instances
#include <click/bighashmap.cc>

#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<Path, PathInfo>;
#endif

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(InOrderQueue)
