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
     _max_tx_packet_ms(0),
     _debug_token(false),
     _threshold(0),
     _timer(this)
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
  _threshold = 0;
  _debug_token = false;
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "PACKET_TIMEOUT", cpUnsigned, "packet timeout", &_max_tx_packet_ms,
		    "THRESHOLD", cpInteger, "packets", &_threshold,
		    "SR", cpElement, "SRForwarder element", &_sr_forwarder,
		    "DEBUG", cpBool, "Debug", &_debug_token,
                    0);

  if (!_max_tx_packet_ms) 
    return errh->error("PACKET_TIMEOUT not specified");

  if (_threshold < 0) 
    return errh->error("THRESHOLD must be specified and > 0");

  if (!_sr_forwarder) 
    return errh->error("SR not specified");
  if (_sr_forwarder->cast("SRForwarder") == 0) 
    return errh->error("SR element is not a SRForwarder");

  timerclear(&_hop_duration);
  /* convehop path_duration from ms to a struct timeval */
  _hop_duration.tv_sec = _max_tx_packet_ms/1000;
  _hop_duration.tv_usec = (_max_tx_packet_ms % 1000) * 1000;

  unsigned int active_duration_ms = 5 * 1000;
  timerclear(&_active_duration);
  /* convehop path_duration from ms to a struct timeval */
  _active_duration.tv_sec = active_duration_ms/1000;
  _active_duration.tv_usec = (active_duration_ms % 1000) * 1000;

  unsigned int clear_duration_ms = 30 * 1000;
  timerclear(&_clear_duration);
  /* convehop path_duration from ms to a struct timeval */
  _clear_duration.tv_sec = clear_duration_ms/1000;
  _clear_duration.tv_usec = (clear_duration_ms % 1000) * 1000;
  
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


  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}

void
SRScheduler::run_timer() 
{
  struct timeval now;
  click_gettimeofday(&now);
  Vector<Path> to_clear;
  Vector<Path> not_active;

  for (STIter iter = _schedules.begin(); iter; iter++) {
    const ScheduleInfo &nfo = iter.value();
    struct timeval expire;
    timeradd(&nfo._last_real, &_active_duration, &expire);
    if (!nfo._active && nfo.clear_timedout()) {
      to_clear.push_back(nfo._p);
    } else if (nfo._active && nfo.active_timedout()) {
      not_active.push_back(nfo._p);
    }
  }

  for (int x = 0; x < not_active.size(); x++) {
    ScheduleInfo *nfo = _schedules.findp(not_active[x]);
    nfo->_active = false;
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " mark_inactive " << path_to_string(nfo->_p);
      click_chatter("%s", sa.take_string().cc());
    }
  }
  for (int x = 0; x < to_clear.size(); x++) {
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " removing " << path_to_string(to_clear[x]);
      click_chatter("%s", sa.take_string().cc());
    }
    _schedules.remove(to_clear[x]);
  }


  _timer.schedule_after_ms(_active_duration.tv_sec/2);
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
  _schedules.insert(p, ScheduleInfo(this));
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
  if (!nfo || !nfo->_active) {
    if (!nfo) {
      nfo = create_nfo(p);
    }
    click_gettimeofday(&nfo->_last_tx);
    nfo->_packets_rx = 0;
    nfo->_seq = pk->seq();
    if (nfo->is_endpoint(_sr_forwarder->ip())) {
      /* make it the other endpoint, the source of this packet */
      nfo->_towards = p[0];
    } else {
      nfo->_towards = p[p.size()-1];
    }

    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " new_path " << path_to_string(p);
      click_chatter("%s", sa.take_string().cc());
    }
  }
  
  if (pk->seq() < nfo->_seq ) {
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " rx_after_timeout";
      sa << " pk->seq " << pk->seq();
      sa << " nfo->seq " << nfo->_seq;
      click_chatter("%s", sa.take_string().cc());
    }
    if (!pk->flag(FLAG_SCHEDULE_FAKE)) {
      output(port).push(p_in);
    }
    return;
  } 
  if (pk->seq() != nfo->_seq && 
      pk->seq() < nfo->_seq + (nfo->_p.size()-1)) {
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " token_dup ";
      sa << " towards  " << p[p.size()-1];
      sa << " expected " << nfo->_towards;
      sa << " pk->seq " << pk->seq();
      sa << " nfo->seq " << nfo->_seq;
      click_chatter("%s", sa.take_string().cc());
    }
    if (!pk->flag(FLAG_SCHEDULE_FAKE)) {
      output(port).push(p_in);
    }
    return;

  } else if (p[p.size()-1] != nfo->_towards && !nfo->is_endpoint(_sr_forwarder->ip())) {
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " rt_reset_detected";
      sa << " towards  " << p[p.size()-1];
      sa << " expected " << nfo->_towards;
      click_chatter("%s", sa.take_string().cc());
    }
      nfo->_towards = p[p.size()-1];
  }

  nfo->_seq = pk->seq();

  if (pk->flag(FLAG_SCHEDULE_TOKEN)) {
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " seq " << nfo->_seq;
      sa << " token_rx";
      sa << " towards " << p[p.size()-1];
      sa << " rx " << nfo->_packets_rx;
      click_chatter("%s", sa.take_string().cc());
    }
    nfo->_token = true;
    if (nfo->is_endpoint(_sr_forwarder->ip())) {
      nfo->_seq = pk->seq() + (nfo->_p.size() - 1);
    } else {
      nfo->_seq = pk->seq() + 1;
    }
  }

  nfo->_packets_rx++;
  click_gettimeofday(&nfo->_last_rx);

  if (nfo->_packets_rx == 1) {
    nfo->_first_rx = nfo->_last_rx;
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " seq " << nfo->_seq;
      sa << " first_rx";
      sa << " towards " << p[p.size()-1];
      sa << " rx " << nfo->_packets_rx;
      click_chatter("%s", sa.take_string().cc());
    }
  }

  if (!pk->flag(FLAG_SCHEDULE_FAKE)) {
    click_gettimeofday(&nfo->_last_real);
    nfo->_active = true;
  } else if (nfo->is_endpoint(_sr_forwarder->ip())) {
    /* i'm an endpoint */
    p_in->kill();
    return;
  }
  output(port).push(p_in);
}

bool 
SRScheduler::ready_for(const Packet *p_in, Path match_this_path) {
  click_ether *eh = (click_ether *) p_in->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  
  if (!pk->flag(FLAG_SCHEDULE)) {
    click_chatter("SRScheduler %s: outgoing schedule flag not set",
		  id().cc());
    return true;
  }
  
  Path p;
  for (int x = 0; x < pk->num_hops(); x++) {
    p.push_back(pk->get_hop(x));
  }
  

  if (match_this_path == p) {
    return true;
  } else  if (match_this_path.size()) {
    return false;
  }
  ScheduleInfo *nfo = find_nfo(p);
  if (!nfo || !nfo->_active) {
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " create:  new_path " << path_to_string(p);
      click_chatter("%s", sa.take_string().cc());
    }
    if (!nfo) {
      nfo = create_nfo(p);
      nfo->_seq = 0;
    }
    nfo->_active = true;
    nfo->_token = true;
    nfo->_towards = p[p.size()-1];
    return true;
  } 
  if (nfo->_towards != p[p.size()-1]) {
    /* this packet is going in the wrong direction */
    return false;
  } 
  if (nfo->_token) {
    return true;
  }
  if (nfo->hop_timedout()) {
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " seq " << nfo->_seq;
      sa << " hop_to  ";
      sa << " towards " << p[p.size()-1].s();
      sa << " rx " << nfo->_packets_rx;
      sa << " last_rx " << now - nfo->_last_rx;
      sa << " first_rx " << now - nfo->_first_rx;
      click_chatter("%s", sa.take_string().cc());
    }
    nfo->_token = true;
    nfo->_towards = p[p.size()-1];
    nfo->_seq = nfo->_seq + 1;
    return true;
  } 
  if (nfo->is_endpoint(_sr_forwarder->ip()) && nfo->rt_timedout()) {
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " seq " << nfo->_seq;
      sa << " rt_to   ";
      sa << " towards " << p[p.size()-1].s();
      sa << " rx " << nfo->_packets_rx;
      sa << " last_tx " << now - nfo->_last_tx;
      click_chatter("%s", sa.take_string().cc());
    }
    nfo->_token = true;
    nfo->_towards = p[p.size()-1];
    nfo->_seq = nfo->_seq + 2*(nfo->_p.size() - 1);
    return true;
  } 
  return false;
}

Packet *
SRScheduler::pull(int)
{
  Packet *p_out = _queue1->yank1(sr_filter(this, Path()));
  IPAddress ip = _sr_forwarder->ip();

  if (!p_out) {
    Path p = Path();
    /* find an expired token */
    struct timeval now;
    click_gettimeofday(&now);
    
    for (STIter iter = _schedules.begin(); iter; iter++) {
      const ScheduleInfo &nfo = iter.value();
      /* 
       * only send a fake if I'm active
       * an endpoint, and the last rx packet wasn't a fake.
       */
      if (nfo._active && 
	  nfo.endpoint_timedout(ip) && 
	  timercmp(&nfo._last_rx, &nfo._last_real, ==)) {
	p = nfo._p;
	break;
      }
    }
    
    if (!p.size()) {
      return (0);
    }

    if (ip == p[p.size()-1]) {
      p = reverse_path(p);
    }
    
    if (ip != p[0]) {
      /* i'm an intermediate hop 
       *  intermediate hop's don't send fake packets
       */
      return (0);
    }

    /* fake up a token packet */
    p_out = _sr_forwarder->encap((0), 0, p, 0);
    if (!p_out) {
      return 0;
    }
    click_ether *eh = (click_ether *) p_out->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
    pk->set_flag(FLAG_SCHEDULE | FLAG_SCHEDULE_TOKEN | FLAG_SCHEDULE_FAKE);
    if (_debug_token) {
      ScheduleInfo *nfo = find_nfo(p);
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " seq " << nfo->_seq;
      sa << " fake    ";
      sa << " towards " << p[p.size()-1].s();
      sa << " rx " << nfo->_packets_rx;
      click_chatter("%s", sa.take_string().cc());
    }
  }
  
  click_ether *eh = (click_ether *) p_out->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  Path p;
  for (int x = 0; x < pk->num_hops(); x++) {
    p.push_back(pk->get_hop(x));
  }

  ScheduleInfo *nfo = find_nfo(p);

  /* send */
  if (!pk->flag(FLAG_SCHEDULE_FAKE)) {
    click_gettimeofday(&nfo->_last_real);
  }
   
  if (nfo->_packets_tx == _threshold || 
      !_queue1->yank1_peek(sr_filter(this, p))) {
    pk->set_flag(FLAG_SCHEDULE_TOKEN);
  }
  if (nfo->_packets_tx == 0 && _debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " seq " << nfo->_seq;
      sa << " first_tx";
      sa << " towards " << p[p.size()-1];
      sa << " rx " << nfo->_packets_rx;
      click_chatter("%s", sa.take_string().cc());
  }


  nfo->_packets_tx++;    
  click_gettimeofday(&nfo->_last_tx);
  pk->set_seq(nfo->_seq);
  if (!pk->flag(FLAG_SCHEDULE_FAKE)) {
    nfo->_active = true;
  }


  if (pk->flag(FLAG_SCHEDULE_TOKEN)) {
    if (_debug_token) {
      struct timeval now;
      click_gettimeofday(&now);
      StringAccum sa;
      sa << "SRScheduler " << now;
      sa << " seq " << nfo->_seq;
      sa << " token_tx";
      sa << " towards " << p[p.size()-1].s();
      sa << " tx " << nfo->_packets_tx;
      sa << " time " << now - nfo->_first_rx;
      click_chatter("%s", sa.take_string().cc());
    }
    
    nfo->_token = false;
    nfo->_packets_tx = 0;
    nfo->_packets_rx = 0;
    nfo->_tokens_passed++;
    if (!nfo->is_endpoint(ip)) {
      nfo->_towards = nfo->other_endpoint(nfo->_towards);
    }
  }
  
  /* send */
  return p_out;
}
  

String
SRScheduler::static_print_debug(Element *f, void *)
{
  StringAccum sa;
  SRScheduler *d = (SRScheduler *) f;
  sa << d->_debug_token << "\n";
  return sa.take_string();
}



String
SRScheduler::static_print_stats(Element *f, void *)
{
  SRScheduler *d = (SRScheduler *) f;
  return d->print_stats();
}

String
SRScheduler::print_stats()
{
  StringAccum sa;

  struct timeval now;
  click_gettimeofday(&now);
  for (STIter iter = _schedules.begin(); iter; iter++) {
    const ScheduleInfo &nfo = iter.value();
    sa << path_to_string(nfo._p) << " :";
    sa << " active " << nfo._active;
    sa << " token " << nfo._token;
    sa << " towards " << nfo._towards;
    sa << " seq " << nfo._seq;
    sa << " packets_tx " << nfo._packets_tx;
    sa << " tokens_passed " << nfo._tokens_passed;
    sa << " first_rx " << now - nfo._first_rx;
    sa << " last_rx " << now - nfo._last_rx;
    sa << " last_tx " << now - nfo._last_tx;
    sa << " last_real " << now - nfo._last_real;
    sa << "\n";
  }

  return sa.take_string();
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

int
SRScheduler::static_write_debug(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  SRScheduler *n = (SRScheduler *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`debug' must be a boolean");

  n->_debug_token = b;
  return 0;
}
void
SRScheduler::add_handlers()
{
  add_write_handler("clear", static_clear, 0);
  add_read_handler("stats", static_print_stats, 0);
  add_write_handler("debug", static_write_debug, 0);
  add_read_handler("debug", static_print_debug, 0);
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
