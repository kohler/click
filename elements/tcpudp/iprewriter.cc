/*
 * iprewriter.{cc,hh} -- rewrites packet source and destination
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "iprewriter.hh"
#include <click/click_ip.h>
#include <click/click_tcp.h>
#include <click/click_udp.h>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/router.hh>
#include <click/llrpc.h>
#include <limits.h>

int IPRewriter::_global_instance_counter = 0;

IPRewriter::IPRewriter()
  : _tcp_map(0), _udp_map(0), _tcp_done(0), 
    _tcp_done_tail(0),
    _tcp_done_gc_timer(tcp_done_gc_hook, this),
    _tcp_gc_timer(tcp_gc_hook, this),
    _udp_gc_timer(udp_gc_hook, this)
{
  MOD_INC_USE_COUNT;
}

IPRewriter::~IPRewriter()
{
  MOD_DEC_USE_COUNT;
  assert(!_tcp_gc_timer.scheduled() && !_udp_gc_timer.scheduled());
}

void *
IPRewriter::cast(const char *n)
{
  if (strcmp(n, "IPRw") == 0)
    return (IPRw *)this;
  else if (strcmp(n, "IPRewriter") == 0)
    return (IPRewriter *)this;
  else
    return 0;
}

void
IPRewriter::notify_noutputs(int n)
{
  if (n < 1)
    set_noutputs(1);
  else if (n > 256)
    set_noutputs(256);
  else
    set_noutputs(n);
}

int
IPRewriter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int before = errh->nerrors();
  int ninputs = 0;
  // numbers in seconds
  _tcp_timeout_jiffies = 86400;		// 24 hours
  _tcp_done_timeout_jiffies = 30;	// 30 seconds
  _udp_timeout_jiffies = 60;		// 1 minute
  _tcp_gc_interval = 3600;		// 1 hour
  _tcp_done_gc_interval = 10;		// 10 seconds
  _udp_gc_interval = 10;		// 10 seconds
  _tcp_done_gc_incr = false;
  
  for (int i = 0; i < conf.size(); i++) {
    if (cp_va_parse_keyword(conf[i], this, errh,
			    "REAP_TCP", cpSeconds, "TCP garbage collection interval", &_tcp_gc_interval,
			    "REAP_TCP_DONE", cpSeconds, "TCP garbage collection interval for completed sessions", &_tcp_done_gc_interval,
			    "REAP_UDP", cpSeconds, "UDP garbage collection interval", &_udp_gc_interval,
			    "TCP_TIMEOUT", cpSeconds, "TCP timeout interval", &_tcp_timeout_jiffies,
			    "TCP_DONE_TIMEOUT", cpSeconds, "Completed TCP timeout interval", &_tcp_done_timeout_jiffies,
			    "UDP_TIMEOUT", cpSeconds, "UDP timeout interval", &_udp_timeout_jiffies,
			    "TCP_DONE_GC_INCR", cpBool, "clean tcp completed sessions incrementally", &_tcp_done_gc_incr,
			    0) != 0)
      continue;
    InputSpec is;
    if (parse_input_spec(conf[i], is, "input spec " + String(i), errh) >= 0) {
      _input_specs.push_back(is);
      ninputs++;
    }
  }

  // change timeouts into jiffies
  _tcp_timeout_jiffies *= CLICK_HZ;
  _tcp_done_timeout_jiffies *= CLICK_HZ;
  _udp_timeout_jiffies *= CLICK_HZ;
  
  if (ninputs == 0)
    return errh->error("too few arguments; expected `INPUTSPEC, ...'");
  set_ninputs(ninputs);
  if (errh->nerrors() == before)
    return 0;
  else {
    uninitialize();
    return -1;
  }
}

int
IPRewriter::initialize(ErrorHandler *)
{
  _instance_index = IPRewriter::_global_instance_counter++;
  _nmapping_failures = 0;

  _tcp_gc_timer.initialize(this);
  _tcp_done_gc_timer.initialize(this);
  _udp_gc_timer.initialize(this);

  _tcp_gc_timer.schedule_after_s(_tcp_gc_interval);
  _udp_gc_timer.schedule_after_s(_udp_gc_interval);
  _tcp_done_gc_timer.schedule_after_s(_tcp_done_gc_interval);

  return 0;
}

void
IPRewriter::uninitialize()
{
  clear_map(_tcp_map);
  clear_map(_udp_map);

  for (int i = 0; i < _input_specs.size(); i++)
    if (_input_specs[i].kind == INPUT_SPEC_PATTERN)
      _input_specs[i].u.pattern.p->unuse();
  _input_specs.clear();
}

int
IPRewriter::notify_pattern(Pattern *p, ErrorHandler *errh)
{
  if (!p->allow_napt())
    return errh->error("IPRewriter cannot accept IPAddrRewriter patterns");
  return IPRw::notify_pattern(p, errh);
}

void
IPRewriter::take_state(Element *e, ErrorHandler *errh)
{
  IPRewriter *rw = (IPRewriter *)e->cast("IPRewriter");
  if (!rw) return;

  if (noutputs() != rw->noutputs()) {
    errh->warning("taking mappings from `%s', although it has %s output ports", rw->declaration().cc(), (rw->noutputs() > noutputs() ? "more" : "fewer"));
    if (noutputs() < rw->noutputs())
      errh->message("(out of range mappings will be dropped)");
  }

  _tcp_map.swap(rw->_tcp_map);
  _udp_map.swap(rw->_udp_map);

  // check rw->_all_patterns against our _all_patterns
  Vector<Pattern *> pattern_map;
  for (int i = 0; i < rw->_all_patterns.size(); i++) {
    Pattern *p = rw->_all_patterns[i], *q = 0;
    for (int j = 0; j < _all_patterns.size() && !q; j++)
      if (_all_patterns[j]->can_accept_from(*p))
	q = _all_patterns[j];
    pattern_map.push_back(q);
  }
  
  take_state_map(_tcp_map, &_tcp_done, &_tcp_done_tail, rw->_all_patterns, pattern_map);
  take_state_map(_udp_map, 0, 0, rw->_all_patterns, pattern_map);
  Mapping *m = _tcp_done;
  Mapping *mp = 0;
  while (m) {
    mp = m;
    m = m->free_next();
  }
  _tcp_done_tail = mp;
}

void
IPRewriter::tcp_gc_hook(Timer *timer, void *thunk)
{
  IPRewriter *rw = (IPRewriter *)thunk;
  unsigned wait = rw->_tcp_gc_interval;
#if IPRW_RWLOCKS
  if (rw->_rwlock.attempt_write()) {
#elif IPRW_SPINLOCKS
  if (rw->_spinlock.attempt()) {
#endif
  rw->clean_map(rw->_tcp_map, click_jiffies() - rw->_tcp_timeout_jiffies);
#if IPRW_RWLOCKS
  rw->_rwlock.release_write();
  } else wait = 1;		// XXX too long a wait?
#elif IPRW_SPINLOCKS
  rw->_spinlock.release();
  } else wait = 1;
#endif
  timer->reschedule_after_s(wait);
}

void
IPRewriter::tcp_done_gc_hook(Timer *timer, void *thunk)
{
  IPRewriter *rw = (IPRewriter *)thunk;
  unsigned wait = rw->_tcp_done_gc_interval;
#if IPRW_RWLOCKS
  if (rw->_rwlock.attempt_write()) {
#elif IPRW_SPINLOCKS
  if (rw->_spinlock.attempt()) {
#endif
  rw->clean_map_free_tracked 
    (rw->_tcp_map, rw->_tcp_done, rw->_tcp_done_tail,
     click_jiffies() - rw->_tcp_done_timeout_jiffies);
#if IPRW_RWLOCKS
  rw->_rwlock.release_write();
  } else wait = 1;		// XXX too long a wait?
#elif IPRW_SPINLOCKS
  rw->_spinlock.release();
  } else wait = 1;
#endif
  timer->reschedule_after_s(wait);
}

void
IPRewriter::udp_gc_hook(Timer *timer, void *thunk)
{
  IPRewriter *rw = (IPRewriter *)thunk;
  unsigned wait = rw->_udp_gc_interval;
#if IPRW_RWLOCKS
  if (rw->_rwlock.attempt_write()) {
#elif IPRW_SPINLOCKS
  if (rw->_spinlock.attempt()) {
#endif
  rw->clean_map(rw->_udp_map, click_jiffies() - rw->_udp_timeout_jiffies);
#if IPRW_RWLOCKS
  rw->_rwlock.release_write();
  } else wait = 1;		// XXX too long a wait?
#elif IPRW_SPINLOCKS
  rw->_spinlock.release();
  } else wait = 1;
#endif
  timer->reschedule_after_s(wait);
}

IPRw::Mapping *
IPRewriter::apply_pattern(Pattern *pattern, int ip_p, const IPFlowID &flow,
			  int fport, int rport)
{
  assert(fport >= 0 && fport < noutputs() && rport >= 0 && rport < noutputs());
  
  if (ip_p != IP_PROTO_TCP && ip_p != IP_PROTO_UDP)
    return 0;
  
  Mapping *forward = new Mapping;
  Mapping *reverse = new Mapping;

  if (forward && reverse) {
    if (!pattern)
      Mapping::make_pair(ip_p, flow, flow, fport, rport, forward, reverse);
    else if (!pattern->create_mapping(ip_p, flow, fport, rport, forward, reverse))
      goto failure;

    IPFlowID reverse_flow = forward->flow_id().rev();
    if (ip_p == IP_PROTO_TCP) {
      _tcp_map.insert(flow, forward);
      _tcp_map.insert(reverse_flow, reverse);
    } else {
      _udp_map.insert(flow, forward);
      _udp_map.insert(reverse_flow, reverse);
    }
    return forward;
  }

 failure:
  _nmapping_failures++;
  delete forward;
  delete reverse;
  return 0;
}

void
IPRewriter::push(int port, Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  IPFlowID flow(p);
  click_ip *iph = p->ip_header();
#if IPRW_RWLOCKS
  bool has_lock = false;
#endif

  // handle non-TCP and non-first fragments
  int ip_p = iph->ip_p;
  if ((ip_p != IP_PROTO_TCP && ip_p != IP_PROTO_UDP) || !IP_FIRSTFRAG(iph)) {
    const InputSpec &is = _input_specs[port];
    if (is.kind == INPUT_SPEC_NOCHANGE)
      output(is.u.output).push(p);
    else
      p->kill();
    return;
  }
 
#if IPRW_RWLOCKS
  _rwlock.acquire_read();
#elif IPRW_SPINLOCKS
  _spinlock.acquire();
#endif
  Mapping *m = (ip_p == IP_PROTO_TCP ? _tcp_map.find(flow) : _udp_map.find(flow));

#if IPRW_RWLOCKS
  _rwlock.release_read();
#endif
  
  if (!m) {			// create new mapping
    const InputSpec &is = _input_specs[port];
#if IPRW_RWLOCKS
    _rwlock.acquire_write();
    has_lock = true;
#endif
    switch (is.kind) {

     case INPUT_SPEC_NOCHANGE:
#if IPRW_RWLOCKS
      _rwlock.release_write();
      has_lock = false;
#elif IPRW_SPINLOCKS
      _spinlock.release();
#endif
      output(is.u.output).push(p);
      return;

     case INPUT_SPEC_DROP:
      break;

     case INPUT_SPEC_KEEP: {
       int fport = is.u.keep.fport;
       int rport = is.u.keep.rport;
       m = IPRewriter::apply_pattern(0, ip_p, flow, fport, rport);
       break;
     }

     case INPUT_SPEC_PATTERN: {
       Pattern *pat = is.u.pattern.p;
       int fport = is.u.pattern.fport;
       int rport = is.u.pattern.rport;
       m = IPRewriter::apply_pattern(pat, ip_p, flow, fport, rport);
       break;
     }

     case INPUT_SPEC_MAPPER: {
       m = is.u.mapper->get_map(this, ip_p, flow, p);
       break;
     }
      
    }
    if (!m) {
#if IPRW_RWLOCKS
      _rwlock.release_write();
      has_lock = false;
#elif IPRW_SPINLOCKS
      _spinlock.release();
#endif
      p->kill();
      return;
    }
  }

  m->apply(p);

  if (ip_p == IP_PROTO_TCP) {
    click_tcp *tcph = p->tcp_header();
    if (tcph->th_flags & (TH_SYN | TH_FIN | TH_RST)) {
      
#if IPRW_RWLOCKS
      if (!has_lock) {
        _rwlock.acquire_write();
        has_lock = true;
      }
#endif
      
      if (_tcp_done_gc_incr && (tcph->th_flags & TH_SYN))
        incr_clean_map_free_tracked 
	  (_tcp_map, _tcp_done, _tcp_done_tail, click_jiffies() - _tcp_done_timeout_jiffies);

      // add to list for dropping TCP connections faster
      if (!m->free_tracked() && (tcph->th_flags & (TH_FIN | TH_RST))
	  && m->session_over())
	m->add_to_free_tracked_tail(_tcp_done, _tcp_done_tail);
    }
  }
  
#if IPRW_RWLOCKS
  if (has_lock) {
    _rwlock.release_write();
    has_lock = false;
  }
#elif IPRW_SPINLOCKS
  _spinlock.release();
#endif
  output(m->output()).push(p);
}


String
IPRewriter::dump_mappings_handler(Element *e, void *thunk)
{
  IPRewriter *rw = (IPRewriter *)e;
  Map *map = (thunk ? &rw->_udp_map : &rw->_tcp_map);
  
#if IPRW_SPINLOCKS
  rw->_spinlock.acquire();
#endif
  StringAccum sa;
  for (Map::Iterator iter = map->first(); iter; iter++) {
    Mapping *m = iter.value();
    if (m->is_primary())
      sa << m->s() << "\n";
  }
#if IPRW_SPINLOCKS
  rw->_spinlock.release();
#endif
  return sa.take_string();
}

String
IPRewriter::dump_tcp_done_mappings_handler(Element *e, void *)
{
  IPRewriter *rw = (IPRewriter *)e;
  
#if IPRW_SPINLOCKS
  rw->_spinlock.acquire();
#endif
  StringAccum sa;
  for (Mapping *m = rw->_tcp_done; m; m = m->free_next()) {
    if (m->session_over())
      sa << m->s() << "\n";
  }
#if IPRW_SPINLOCKS
  rw->_spinlock.release();
#endif
  return sa.take_string();
}

String
IPRewriter::dump_nmappings_handler(Element *e, void *thunk)
{
  IPRewriter *rw = (IPRewriter *)e;
  if (!thunk)
    return String(rw->_tcp_map.size()) + " " + String(rw->_udp_map.size()) + "\n";
  else
    return String(rw->_nmapping_failures) + "\n";
}

String
IPRewriter::dump_patterns_handler(Element *e, void *)
{
  IPRewriter *rw = (IPRewriter *)e;
  String s;
#if IPRW_SPINLOCKS
  rw->_spinlock.acquire();
#endif
  for (int i = 0; i < rw->_input_specs.size(); i++)
    if (rw->_input_specs[i].kind == INPUT_SPEC_PATTERN)
      s += rw->_input_specs[i].u.pattern.p->s() + "\n";
#if IPRW_SPINLOCKS
  rw->_spinlock.release();
#endif
  return s;
}

void
IPRewriter::add_handlers()
{
  add_read_handler("tcp_mappings", dump_mappings_handler, (void *)0);
  add_read_handler("udp_mappings", dump_mappings_handler, (void *)1);
  add_read_handler("tcp_done_mappings", dump_tcp_done_mappings_handler, 0);
  add_read_handler("nmappings", dump_nmappings_handler, (void *)0);
  add_read_handler("mapping_failures", dump_nmappings_handler, (void *)1);
  add_read_handler("patterns", dump_patterns_handler, (void *)0);
}

int
IPRewriter::llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_IPREWRITER_MAP_TCP) {

    // Data	: unsigned saddr, daddr; unsigned short sport, dport
    // Incoming : the flow ID
    // Outgoing : If there is a mapping for that flow ID, then stores the
    //		  mapping into 'data' and returns zero. Otherwise, returns
    //		  -EAGAIN.

    IPFlowID flowid;
    if (CLICK_LLRPC_GET_DATA(&flowid, data, sizeof(IPFlowID)) < 0)
      return -EFAULT;
#if IPRW_SPINLOCKS
    _spinlock.acquire();
#endif
    Mapping *m = get_mapping(IP_PROTO_TCP, flowid);
    if (!m) {
#if IPRW_SPINLOCKS
      _spinlock.release();
#endif
      return -EAGAIN;
    }
    flowid = m->flow_id();
#if IPRW_SPINLOCKS
    _spinlock.release();
#endif
    if (CLICK_LLRPC_PUT_DATA(data, &flowid, sizeof(IPFlowID)) < 0)
      return -EFAULT;
    return 0;
    
  } else if (command == CLICK_LLRPC_IPREWRITER_MAP_UDP) {

    // Data	: unsigned saddr, daddr; unsigned short sport, dport
    // Incoming : the flow ID
    // Outgoing : If there is a mapping for that flow ID, then stores the
    //		  mapping into 'data' and returns zero. Otherwise, returns
    //		  -EAGAIN.

    IPFlowID flowid;
    if (CLICK_LLRPC_GET_DATA(&flowid, data, sizeof(IPFlowID)) < 0)
      return -EFAULT;
#if IPRW_SPINLOCKS
    _spinlock.acquire();
#endif
    Mapping *m = get_mapping(IP_PROTO_UDP, flowid);
    if (!m) {
#if IPRW_SPINLOCKS
      _spinlock.release();
#endif
      return -EAGAIN;
    }
    flowid = m->flow_id();
#if IPRW_SPINLOCKS
    _spinlock.release();
#endif
    if (CLICK_LLRPC_PUT_DATA(data, &flowid, sizeof(IPFlowID)) < 0)
      return -EFAULT;
    return 0;
    
  } else
    return Element::llrpc(command, data);
}

ELEMENT_REQUIRES(IPRw IPRewriterPatterns)
EXPORT_ELEMENT(IPRewriter)

#include <click/bighashmap.cc>
#include <click/vector.cc>
