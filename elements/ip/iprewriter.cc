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
#include "elements/ip/iprwpatterns.hh"
#include <click/click_ip.h>
#include <click/click_tcp.h>
#include <click/click_udp.h>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/llrpc.h>

#include <limits.h>

IPRewriter::IPRewriter()
  : _tcp_map(0), _udp_map(0), _timer(this)
{
  // no MOD_INC_USE_COUNT; rely on IPRw
}

IPRewriter::~IPRewriter()
{
  // no MOD_DEC_USE_COUNT; rely on IPRw
  assert(!_timer.scheduled());
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
  set_noutputs(n < 1 ? 1 : n);
}

int
IPRewriter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() == 0)
    return errh->error("too few arguments; expected `INPUTSPEC, ...'");
  set_ninputs(conf.size());

  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    InputSpec is;
    if (parse_input_spec(conf[i], is, "input spec " + String(i), errh) >= 0)
      _input_specs.push_back(is);
  }
  return (errh->nerrors() == before ? 0 : -1);
}

int
IPRewriter::initialize(ErrorHandler *errh)
{
  _timer.attach(this);
  _timer.schedule_after_ms(GC_INTERVAL_SEC * 1000);
#if defined(CLICK_LINUXMODULE) && !defined(HAVE_TCP_PROT)
  errh->message
    ("The kernel does not export the symbol `tcp_prot', so I cannot remove\n"
     "stale mappings. Apply the Click kernel patch to fix this problem.");
#endif
#ifndef CLICK_LINUXMODULE
  errh->message("can't remove stale mappings at userlevel");
#endif
  return 0;
}

void
IPRewriter::uninitialize()
{
  _timer.unschedule();

  clear_map(_tcp_map);
  clear_map(_udp_map);

  for (int i = 0; i < _input_specs.size(); i++)
    if (_input_specs[i].kind == INPUT_SPEC_PATTERN)
      _input_specs[i].u.pattern.p->unuse();
  _input_specs.clear();
}

void
IPRewriter::take_state(Element *e, ErrorHandler *errh)
{
  IPRewriter *rw = (IPRewriter *)e->cast("IPRewriter");
  if (!rw) return;

  if (noutputs() != rw->noutputs()) {
    errh->warning("taking mappings from `%s', although it has\n%s output ports", rw->declaration().cc(), (rw->noutputs() > noutputs() ? "more" : "fewer"));
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
IPRewriter::run_scheduled()
{
#if defined(CLICK_LINUXMODULE) && defined(HAVE_TCP_PROT)
  mark_live_tcp(_tcp_map);
#endif
  clean_map(_tcp_map);
  clean_map(_udp_map);
  _timer.schedule_after_ms(GC_INTERVAL_SEC * 1000);
}

IPRw::Mapping *
IPRewriter::apply_pattern(Pattern *pattern, int fport, int rport,
			  bool is_tcp, const IPFlowID &flow)
{
  assert(fport >= 0 && fport < noutputs() && rport >= 0 && rport < noutputs());
  Mapping *forward = new Mapping;
  Mapping *reverse = new Mapping;

  if (forward && reverse) {
    if (!pattern)
      Mapping::make_pair(flow, flow, fport, rport, forward, reverse);
    else if (!pattern->create_mapping(flow, fport, rport, forward, reverse))
      goto failure;

    IPFlowID reverse_flow = forward->flow_id().rev();
    if (is_tcp) {
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
  bool tcp = iph->ip_p == IP_PROTO_TCP;
  if ((!tcp && iph->ip_p != IP_PROTO_UDP) || !IP_FIRSTFRAG(iph)) {
    const InputSpec &is = _input_specs[port];
    if (is.kind == INPUT_SPEC_NOCHANGE)
      output(is.u.output).push(p);
    else
      p->kill();
    return;
  }
  
  Mapping *m = (tcp ? _tcp_map.find(flow) : _udp_map.find(flow));
  
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
       m = IPRewriter::apply_pattern(0, fport, rport, tcp, flow);
       break;
     }

     case INPUT_SPEC_PATTERN: {
       Pattern *pat = is.u.pattern.p;
       int fport = is.u.pattern.fport;
       int rport = is.u.pattern.rport;
       m = IPRewriter::apply_pattern(pat, fport, rport, tcp, flow);
       break;
     }

     case INPUT_SPEC_MAPPER: {
       m = is.u.mapper->get_map(this, tcp, flow);
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
    Mapping *m = get_mapping(true, flowid);
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
    Mapping *m = get_mapping(false, flowid);
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
