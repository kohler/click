/*
 * SRScheduler.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussrschedulers Institute of Technology
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
#include <elements/grid/linktable.hh>
#include <elements/grid/arptable.hh>
#include <elements/grid/sr/path.hh>
#include <elements/grid/sr/srcrstat.hh>
#include <elements/standard/notifierqueue.hh>
#include "srscheduler.hh"
#include "srforwarder.hh"
#include "srpacket.hh"
#include <elements/standard/pullswitch.hh>
#include <click/router.hh>
#include <click/llrpc.h>

CLICK_DECLS

#ifndef srscheduler_assert
#define srscheduler_assert(e) ((e) ? (void) 0 : srscheduler_assert_(__FILE__, __LINE__, #e))
#endif /* srcr_assert */


SRScheduler::SRScheduler()
  :  Element(2,2),
     _debug_token(false)
{
  MOD_INC_USE_COUNT;
}

SRScheduler::~SRScheduler()
{
  MOD_DEC_USE_COUNT;
}

int
SRScheduler::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  unsigned int hop_duration_ms = 0;
  unsigned int rt_duration_ms = 0;
  unsigned int endpoint_duration_ms = 0;
  unsigned int real_duration_ms = 0;
  _threshold = 0;
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "RT_TIMEOUT", cpUnsigned, "ms", &rt_duration_ms,
		    "HOP_TIMEOUT", cpUnsigned, "ms", &hop_duration_ms,
		    "ENDPOINT_TIMEOUT", cpUnsigned, "ms", &endpoint_duration_ms,
		    "REAL_TIMEOUT", cpUnsigned, "ms", &real_duration_ms,
		    "THRESHOLD", cpInteger, "packets", &_threshold,
		    "SR", cpElement, "SRForwarder element", &_sr_forwarder,
		    "DEBUG", cpBool, "Debug", &_debug_token,
                    0);

  if (!rt_duration_ms) 
    return errh->error("RT_TIMEOUT not specified");
  if (!hop_duration_ms) 
    return errh->error("HOP_TIMEOUT not specified");
  if (!endpoint_duration_ms) 
    return errh->error("ENDPOINT_TIMEOUT not specified");
  if (!real_duration_ms) 
    return errh->error("REAL_TIMEOUT not specified");
  if (_threshold < 0) 
    return errh->error("HOP_TIMEOUT must be specified and > 0");
  if (!_sr_forwarder) 
    return errh->error("SR not specified");

  if (_sr_forwarder->cast("SRForwarder") == 0) 
    return errh->error("SR element is not a SRForwarder");
    
  timerclear(&_rt_duration);
  /* convert path_duration from ms to a struct timeval */
  _rt_duration.tv_sec = rt_duration_ms/1000;
  _rt_duration.tv_usec = (rt_duration_ms % 1000) * 1000;

  timerclear(&_hop_duration);
  /* convehop path_duration from ms to a struct timeval */
  _hop_duration.tv_sec = hop_duration_ms/1000;
  _hop_duration.tv_usec = (hop_duration_ms % 1000) * 1000;


  timerclear(&_endpoint_duration);
  /* convehop path_duration from ms to a struct timeval */
  _endpoint_duration.tv_sec = endpoint_duration_ms/1000;
  _endpoint_duration.tv_usec = (endpoint_duration_ms % 1000) * 1000;

  timerclear(&_real_duration);
  /* convehop path_duration from ms to a struct timeval */
  _real_duration.tv_sec = real_duration_ms/1000;
  _real_duration.tv_usec = (real_duration_ms % 1000) * 1000;
  
  return ret;
}

SRScheduler *
SRScheduler::clone () const
{
  return new SRScheduler;
}

int
SRScheduler::initialize (ErrorHandler *errh)
{
  
  // Find the next queues
  Vector<Element *> _queue_elements;
  _queues.clear();
  _queue1 = 0;
  
  if (!_queue_elements.size()) {
    CastElementFilter filter("Queue");
    int ok;
    ok = router()->upstream_elements(this, 1, &filter, _queue_elements);
    if (ok < 0)
      return errh->error("flow-based router context failure");
    filter.filter(_queue_elements);
  }
  
  if (_queue_elements.size() == 0)
    return errh->error("no Queues upstream");
  for (int i = 0; i < _queue_elements.size(); i++)
    if (NotifierQueue *s = (NotifierQueue *)_queue_elements[i]->cast("Queue"))
      _queues.push_back(s);
    else
      errh->error("`%s' is not a Storage element", _queue_elements[i]->id().cc());
  if (_queues.size() != _queue_elements.size())
    return -1;
  else if (_queues.size() == 1)
    _queue1 = _queues[0];
  else 
    errh->error("more than 1 upstream q");

  return 0;
}

SRScheduler::ScheduleInfo *
SRScheduler::find_nfo(Path p) {
  Path rev = reverse_path(p);
  ScheduleInfo *nfo = _schedules.findp(p);
  ScheduleInfo *rev_info = _schedules.findp(rev);
  
  if (nfo || rev_info) {
    if (!nfo) {
      nfo = rev_info;
    }
    return nfo;
  }

  return (0);
}

SRScheduler::ScheduleInfo *
SRScheduler::create_nfo(Path p) 
{
  _schedules.insert(p, ScheduleInfo());
  ScheduleInfo *nfo = _schedules.findp(p);
  nfo->_p = p;
  return nfo;
}
void
SRScheduler::push(int port, Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  
  if (!pk->flag(FLAG_SCHEDULE)) {
    click_chatter("SRScheduler %s: incoming schedule flag not set",
		  id().cc());
    output(port).push(p_in);
    return;
  }
  
  Path p;
  for (int x = 0; x < pk->num_hops(); x++) {
    p.push_back(pk->get_hop(x));
  }

  ScheduleInfo *nfo = find_nfo(p);
  if (!nfo) {
    nfo = create_nfo(p);
    click_gettimeofday(&nfo->_last_tx);
  }
  if (!nfo->_token) {
    nfo->_token = pk->flag(FLAG_SCHEDULE_TOKEN);
  }
  if (pk->flag(FLAG_SCHEDULE_TOKEN)) {
    if (_debug_token) {
      click_chatter("SRScheduler %s: got token\n",
		    id().cc());
    }
  }
  click_gettimeofday(&nfo->_last_rx);
  IPAddress ip = _sr_forwarder->ip();
  if (pk->flag(FLAG_SCHEDULE_FAKE) && 
      (p[0] == ip || p[p.size()-1] == ip)) {
    /* i'm an endpoint */
    p_in->kill();
    return;
  } else {
    click_gettimeofday(&nfo->_last_real);
  }
  output(port).push(p_in);
}
Packet *
SRScheduler::pull(int)
{
  Packet *head = _queue1->head();

  if (head) {

    click_ether *eh = (click_ether *) head->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);

    if (!pk->flag(FLAG_SCHEDULE)) {
      click_chatter("SRScheduler %s: outgoing schedule flag not set",
		    id().cc());
      return input(1).pull();
    }

    Path p;
    for (int x = 0; x < pk->num_hops(); x++) {
      p.push_back(pk->get_hop(x));
    }
    
    ScheduleInfo *nfo = find_nfo(p);
    if (!nfo) {
      nfo = create_nfo(p);
      nfo->_token = true;
      if (_debug_token) {
	click_chatter("SRScheduler %s: create_token: new_path\n",
		      id().cc());
      }
    } else {   
      struct timeval hop_expire;
      struct timeval rt_expire;
      struct timeval now;
      click_gettimeofday(&now);
      timeradd(&nfo->_last_rx, &_hop_duration, &hop_expire);
      timeradd(&nfo->_last_tx, &_rt_duration, &rt_expire);
      if (timercmp(&nfo->_last_rx, &nfo->_last_tx, >) && 
	  timercmp(&hop_expire, &now, <)) {
	if (_debug_token) {
	  click_chatter("SRScheduler %s: create_token: hop_timeout\n",
			id().cc());
	}
	nfo->_token = true;
      } else if (timercmp(&rt_expire, &now, <)) {
	  if (_debug_token) {
	    click_chatter("SRScheduler %s: create_token: rt_timeout\n",
			  id().cc());
	  }
	nfo->_token = true;
      }
    }
  
    if (nfo->_token) {
      /* send */
      nfo->_packets_sent++;    
      if (nfo->_packets_sent == _threshold || _queue1->size() < 2) {
	pk->set_flag(FLAG_SCHEDULE_TOKEN);
	if (_debug_token) {
	  click_chatter("SRScheduler %s: pass_token\n",
			id().cc());
	}
	nfo->_token = false;
	nfo->_packets_sent = 0;
      }
      click_gettimeofday(&nfo->_last_tx);
      click_gettimeofday(&nfo->_last_real);
      return input(1).pull();
    }
    return (0);
  } 

  Path p = Path();
  struct timeval now;
  click_gettimeofday(&now);
  /* find an expired token */
  for (STIter iter = _schedules.begin(); iter; iter++) {
    ScheduleInfo nfo = iter.value();
    struct timeval expire;
    timeradd(&nfo._last_rx, &_endpoint_duration, &expire);
    if (nfo._token && timercmp(&expire, &now, <)) {
      p = nfo._p;
      break;
    }
  }
  
  if (!p.size()) {
    return (0);
  }
  IPAddress ip = _sr_forwarder->ip();
  if (ip == p[p.size()-1]) {
    p = reverse_path(p);
  }

  ScheduleInfo *nfo = find_nfo(p);
  
  struct timeval expire;
  timeradd(&nfo->_last_real, &_real_duration, &expire);
  if (timercmp(&expire, &now, <)) {
    _schedules.remove(nfo->_p);
    if (_debug_token) {
      click_chatter("SRScheduler %s: delete_token\n",
		    id().cc());
    }
    return (0);
  }
  
  if (ip != p[0]) {
    /* i'm an intermediate hop */
    return (0);
  }





  if (_debug_token) {
    click_chatter("SRScheduler %s: pass_token: fake\n",
		  id().cc());
  }

  /* fake up a token packet */
  Packet *p_out = _sr_forwarder->encap((0), 0, p, 0);
  click_ether *eh = (click_ether *) p_out->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  pk->set_flag(FLAG_SCHEDULE | FLAG_SCHEDULE_TOKEN | FLAG_SCHEDULE_FAKE);

  /* send */
  nfo->_packets_sent++;    
  nfo->_token = false;
  nfo->_packets_sent = 0;
  return p_out;
}

int
SRScheduler::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  SRScheduler *n = (SRScheduler *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}

void
SRScheduler::clear() 
{
  
  struct timeval now;
  click_gettimeofday(&now);
  Vector<Path> to_clear;
  for (STIter iter = _schedules.begin(); iter; iter++) {
    ScheduleInfo nfo = iter.value();
    to_clear.push_back(nfo._p);
  }

  for (int x = 0; x < to_clear.size(); x++) {
    click_chatter("SRScheduler %s: removing %s\n", 
		  id().cc(),
		  path_to_string(to_clear[x]).cc());
    _schedules.remove(to_clear[x]);
  }
}

void
SRScheduler::add_handlers()
{
  add_write_handler("clear", static_clear, 0);
}

void
SRScheduler::srscheduler_assert_(const char *file, int line, const char *expr) const
{
  click_chatter("SRScheduler %s assertion \"%s\" failed: file %s, line %d",
		id().cc(), expr, file, line);
#ifdef CLICK_USERLEVEL  
  abort();
#else
  click_chatter("Continuing execution anyway, hold on to your hats!\n");
#endif

}

// generate Vector template instance
#include <click/vector.cc>
#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<IPAddress, Path>;
template class BigHashMap<Path, ScheduleInfo>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(SRScheduler)
