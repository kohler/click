/*
 * iprewriter.{cc,hh} -- rewrites packet source and destination
 * Max Poletto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "iprewriter.hh"
#include <click/click_ip.h>
#include <click/click_tcp.h>
#include <click/click_udp.h>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/llrpc.h>

#include <limits.h>

IPRewriter::IPRewriter()
  : _tcp_map(0), _udp_map(0), _tcp_done(0),
    _tcp_done_gc_timer(tcp_done_gc_hook, (unsigned long) this),
    _tcp_gc_timer(tcp_gc_hook, (unsigned long) this),
    _udp_gc_timer(udp_gc_hook, (unsigned long) this)
{
  // no MOD_INC_USE_COUNT; rely on IPRw
}

IPRewriter::~IPRewriter()
{
  // no MOD_DEC_USE_COUNT; rely on IPRw
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
IPRewriter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int before = errh->nerrors();
  int ninputs = 0;
  _tcp_gc_interval = 86400000;	// 24 hours
  _tcp_done_gc_interval = 240000;	// 4 minutes
  _udp_gc_interval = 60000;	// 1 minute
  
  for (int i = 0; i < conf.size(); i++) {
    if (cp_va_parse_keyword(conf[i], this, errh,
			    "TCP_GC_INTERVAL", cpMilliseconds, "TCP garbage collection interval", &_tcp_gc_interval,
			    "UDP_GC_INTERVAL", cpMilliseconds, "UDP garbage collection interval", &_udp_gc_interval,
			    0) != 0)
      continue;
    InputSpec is;
    if (parse_input_spec(conf[i], is, "input spec " + String(i), errh) >= 0) {
      _input_specs.push_back(is);
      ninputs++;
    }
  }

  if (ninputs == 0)
    return errh->error("too few arguments; expected `INPUTSPEC, ...'");
  set_ninputs(ninputs);
  return (errh->nerrors() == before ? 0 : -1);
}

int
IPRewriter::initialize(ErrorHandler *)
{
  _tcp_gc_timer.attach(this);
  _tcp_gc_timer.schedule_after_ms(_tcp_gc_interval);
  _udp_gc_timer.attach(this);
  _udp_gc_timer.schedule_after_ms(_udp_gc_interval);
  return 0;
}

void
IPRewriter::uninitialize()
{
  _tcp_gc_timer.unschedule();
  _udp_gc_timer.unschedule();

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
  
  take_state_map(_tcp_map, rw->_all_patterns, pattern_map);
  take_state_map(_udp_map, rw->_all_patterns, pattern_map);
}

void
IPRewriter::tcp_gc_hook(unsigned long thunk)
{
  IPRewriter *rw = (IPRewriter *)thunk;
  rw->clean_map(rw->_tcp_map);
  rw->_tcp_gc_timer.schedule_after_ms(rw->_tcp_gc_interval);
}

void
IPRewriter::tcp_done_gc_hook(unsigned long thunk)
{
  IPRewriter *rw = (IPRewriter *)thunk;
  rw->clean_map_free_tracked(rw->_udp_map, &rw->_tcp_done);
  rw->_tcp_done_gc_timer.schedule_after_ms(rw->_tcp_done_gc_interval);
}

void
IPRewriter::udp_gc_hook(unsigned long thunk)
{
  IPRewriter *rw = (IPRewriter *)thunk;
  rw->clean_map(rw->_udp_map);
  rw->_udp_gc_timer.schedule_after_ms(rw->_udp_gc_interval);
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
  
  Mapping *m = (ip_p == IP_PROTO_TCP ? _tcp_map.find(flow) : _udp_map.find(flow));
  
  if (!m) {			// create new mapping
    const InputSpec &is = _input_specs[port];
    switch (is.kind) {

     case INPUT_SPEC_NOCHANGE:
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
      p->kill();
      return;
    }
  }
  
  m->apply(p);
  output(m->output()).push(p);

  // add to list for dropping TCP connections faster
  if (ip_p == IP_PROTO_TCP && !m->free_tracked()) {
    click_tcp *tcph = reinterpret_cast<click_tcp *>(p->transport_header());
    if ((tcph->th_flags & (TH_FIN | TH_RST)) && m->session_over())
      _tcp_done = m->add_to_free_tracked(_tcp_done);
  }
}


String
IPRewriter::dump_mappings_handler(Element *e, void *)
{
  IPRewriter *rw = (IPRewriter *)e;
  
  StringAccum tcps;
  for (Map::Iterator iter = rw->_tcp_map.first(); iter; iter++) {
    Mapping *m = iter.value();
    if (!m->is_reverse())
      tcps << m->s() << "\n";
  }

  StringAccum udps;
  for (Map::Iterator iter = rw->_udp_map.first(); iter; iter++) {
    Mapping *m = iter.value();
    if (!m->is_reverse())
      udps << m->s() << "\n";
  }

  if (tcps.length() && udps.length())
    return "TCP:\n" + tcps.take_string() + "\nUDP:\n" + udps.take_string();
  else if (tcps.length())
    return "TCP:\n" + tcps.take_string();
  else if (udps.length())
    return "UDP:\n" + udps.take_string();
  else
    return String();
}

String
IPRewriter::dump_nmappings_handler(Element *e, void *)
{
  IPRewriter *rw = (IPRewriter *)e;
  return String(rw->_tcp_map.size()) + " " + String(rw->_udp_map.size()) + "\n";
}

String
IPRewriter::dump_patterns_handler(Element *e, void *)
{
  IPRewriter *rw = (IPRewriter *)e;
  String s;
  for (int i = 0; i < rw->_input_specs.size(); i++)
    if (rw->_input_specs[i].kind == INPUT_SPEC_PATTERN)
      s += rw->_input_specs[i].u.pattern.p->s() + "\n";
  return s;
}

void
IPRewriter::add_handlers()
{
  add_read_handler("mappings", dump_mappings_handler, (void *)0);
  add_read_handler("nmappings", dump_nmappings_handler, (void *)0);
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
    Mapping *m = get_mapping(IP_PROTO_TCP, flowid);
    if (!m)
      return -EAGAIN;
    flowid = m->flow_id();
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
    Mapping *m = get_mapping(IP_PROTO_UDP, flowid);
    if (!m)
      return -EAGAIN;
    flowid = m->flow_id();
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
