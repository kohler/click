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
#include <elements/grid/sr/path.hh>
#include <elements/standard/notifierqueue.hh>
#include <elements/grid/sr/srforwarder.hh>
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
    _timer.schedule_after_ms(10000);
}

bool
InOrderQueue::ready_for(const Packet *p_in) {

    if (p_in->ip_header()->ip_p != IP_PROTO_TCP) {
	/* non tcp packets always just go */
	return true;
    }

    IPFlowID flowid = IPFlowID(p_in);
    tcp_seq_t th_seq = p_in->tcp_header()->th_seq;
    FlowTableEntry *match = _flows.findp(flowid);
    struct timeval age = match->last_tx_age();

    if (th_seq == match->_th_seq) {
	return true;
    }

    return timercmp(&age, &_packet_timeout, >);
    
}

Packet *
InOrderQueue::pull(int)
{
    Packet *packet = NULL;
    packet = yank1(yank_filter(this));

    if (!packet) {
	sleep_listeners();
    }
    return packet;
}



int 
InOrderQueue::bubble_up(Packet *p_in)
{
    bool reordered = false;
    if (p_in->ip_header()->ip_p == IP_PROTO_TCP) {
	IPFlowID id = IPFlowID(p_in);
	const click_tcp *tcph = p_in->tcp_header();
	

	for (int x = _head; x != _tail; x = next_i(x)) {
	    IPFlowID id2 = IPFlowID(_q[x]);
	    if (_q[x]->ip_header()->ip_p == IP_PROTO_TCP && id == id2) {
		const click_tcp *tcph2 = _q[x]->tcp_header();
		if (tcph->th_seq == tcph2->th_seq) {
		    /* packet dup */
		    return 0;
		} else if (SEQ_LT(tcph->th_seq, tcph2->th_seq)) {
		    if (!reordered) {
			reordered = true;
			struct timeval now;
			click_gettimeofday(&now);
			StringAccum sa;
			sa << this;
			sa << " " << id;
			sa << " reordering ";
			sa << " tcph->seq " << tcph->th_seq;
			sa << " tcph2->seq " << tcph2->th_seq;
			click_chatter("%s", sa.take_string().cc());
		    }
		    Packet *tmp = _q[x];
		    _q[x] = p_in;
		    p_in = tmp;
		}
		
	    }
	    
	}
    }

    if (!enq(p_in) && _drops == 0) {
	click_chatter("%{element}: overflow", this);
	_drops++;
    }
    return 0;

}

void
InOrderQueue::push(int, Packet *p_in)
{
    bubble_up(p_in);
    /* there is work to be done! */
    wake_listeners();
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
  for(FlowIter iter = _flows.begin(); iter; iter++) {
    FlowTableEntry f = iter.value();
    struct timeval age = f.last_tx_age();
    sa << f._id << " seq " << f._th_seq << " age " << age << "\n";
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
template class BigHashMap<Path, PathInfo>;
#endif

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(InOrderQueue)
