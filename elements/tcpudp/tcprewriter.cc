/*
 * tcprewriter.{cc,hh} -- rewrites packet source and destination
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "tcprewriter.hh"
#include <click/click_ip.h>
#include <click/click_tcp.h>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/llrpc.h>

#include <limits.h>

// TCPMapping

TCPRewriter::TCPMapping::TCPMapping(bool dst_anno)
  : Mapping(dst_anno), _seqno_delta(0), _ackno_delta(0), _interesting_seqno(0)
{
}

void
TCPRewriter::TCPMapping::change_udp_csum_delta(unsigned old_word, unsigned new_word)
{
  const unsigned short *source_words = (const unsigned short *)&old_word;
  const unsigned short *dest_words = (const unsigned short *)&new_word;
  unsigned delta = _udp_csum_delta;
  for (int i = 0; i < 2; i++) {
    delta += ~source_words[i] & 0xFFFF;
    delta += dest_words[i];
  }
  // why is this required here, but not elsewhere when we do
  // incremental updates?
  if ((int)ntohl(old_word) >= 0 && (int)ntohl(new_word) < 0)
    delta -= htons(1);
  else if ((int)ntohl(old_word) < 0 && (int)ntohl(new_word) >= 0)
    delta += htons(1);
  delta = (delta & 0xFFFF) + (delta >> 16);
  _udp_csum_delta = delta + (delta >> 16);
}

void
TCPRewriter::TCPMapping::apply(WritablePacket *p)
{
  click_ip *iph = p->ip_header();
  assert(iph);
  
  // IP header
  iph->ip_src = _mapto.saddr();
  iph->ip_dst = _mapto.daddr();
  if (_dst_anno)
    p->set_dst_ip_anno(_mapto.daddr());

  unsigned sum = (~iph->ip_sum & 0xFFFF) + _ip_csum_delta;
  sum = (sum & 0xFFFF) + (sum >> 16);
  iph->ip_sum = ~(sum + (sum >> 16));

  mark_used();
  
  // end if not first fragment
  if (!IP_FIRSTFRAG(iph))
    return;
  
  // TCP header
  click_tcp *tcph = p->tcp_header();
  tcph->th_sport = _mapto.sport();
  tcph->th_dport = _mapto.dport();

  // update sequence numbers
  unsigned short csum_delta = _udp_csum_delta;
  if (_seqno_delta) {
    unsigned oldval = ntohl(tcph->th_seq);
    unsigned newval = oldval + _seqno_delta;
    if ((int)oldval < 0 && (int)newval >= 0)
      csum_delta -= htons(1);
    tcph->th_seq = htonl(newval);
  }
  if (_ackno_delta) {
    unsigned oldval = ntohl(tcph->th_ack);
    unsigned newval = oldval + _ackno_delta;
    if ((int)oldval < 0 && (int)newval >= 0)
      csum_delta -= htons(1);
    tcph->th_ack = htonl(newval);
  }

  // update checksum
  unsigned sum2 = (~tcph->th_sum & 0xFFFF) + csum_delta;
  sum2 = (sum2 & 0xFFFF) + (sum2 >> 16);
  tcph->th_sum = ~(sum2 + (sum2 >> 16));

  // check for session ending flags
  if (tcph->th_flags & TH_RST)
    set_session_over();
  else if (tcph->th_flags & TH_FIN)
    set_session_flow_over();
  else if (tcph->th_flags & TH_SYN)
    clear_session_flow_over();
}

String
TCPRewriter::TCPMapping::s() const
{
  StringAccum sa;
  sa << reverse()->flow_id().rev().s() << " => " << flow_id().s()
     << " seq " << (_seqno_delta > 0 ? "+" : "") << _seqno_delta
     << " ack " << (_ackno_delta > 0 ? "+" : "") << _ackno_delta
     << " [" + String(output()) + "]";
  return sa.take_string();
}


// TCPRewriter

TCPRewriter::TCPRewriter()
  : _tcp_map(0), _tcp_done(0), _tcp_done_tail(0),
    _tcp_gc_timer(tcp_gc_hook, this),
    _tcp_done_gc_timer(tcp_done_gc_hook, this)
{
  // no MOD_INC_USE_COUNT; rely on IPRw
}

TCPRewriter::~TCPRewriter()
{
  // no MOD_DEC_USE_COUNT; rely on IPRw
  assert(!_tcp_gc_timer.scheduled() && !_tcp_done_gc_timer.scheduled());
}

void *
TCPRewriter::cast(const char *n)
{
  if (strcmp(n, "IPRw") == 0)
    return (IPRw *)this;
  else if (strcmp(n, "TCPRewriter") == 0)
    return (TCPRewriter *)this;
  else
    return 0;
}

void
TCPRewriter::notify_noutputs(int n)
{
  if (n < 1)
    set_noutputs(1);
  else if (n > 256)
    set_noutputs(256);
  else
    set_noutputs(n);
}

int
TCPRewriter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int before = errh->nerrors();
  int ninputs = 0;
  // numbers in seconds
  _tcp_timeout_jiffies = 86400;		// 24 hours
  _tcp_done_timeout_jiffies = 240;	// 4 minutes
  _tcp_gc_interval = 3600;		// 1 hour
  _tcp_done_gc_interval = 10;		// 10 seconds
  _dst_anno = true;

  if (cp_va_parse_remove_keywords
      (conf, 0, this, errh,
       "REAP_TCP", cpSeconds, "reap interval for active TCP connections", &_tcp_gc_interval,
       "REAP_TCP_DONE", cpSeconds, "reap interval for completed TCP connections", &_tcp_done_gc_interval,
       "TCP_TIMEOUT", cpSeconds, "TCP timeout interval", &_tcp_timeout_jiffies,
       "TCP_DONE_TIMEOUT", cpSeconds, "completed TCP timeout interval", &_tcp_done_timeout_jiffies,
       "DST_ANNO", cpBool, "set destination IP addr annotation?", &_dst_anno,
       0) < 0)
    return -1;

  for (int i = 0; i < conf.size(); i++) {
    InputSpec is;
    if (parse_input_spec(conf[i], is, "input spec " + String(i), errh) >= 0) {
      _input_specs.push_back(is);
      ninputs++;
    }
  }

  // change timeouts into jiffies
  _tcp_timeout_jiffies *= CLICK_HZ;
  _tcp_done_timeout_jiffies *= CLICK_HZ;

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
TCPRewriter::initialize(ErrorHandler *)
{
  _tcp_gc_timer.initialize(this);
  _tcp_gc_timer.schedule_after_s(_tcp_gc_interval);
  _tcp_done_gc_timer.initialize(this);
  _tcp_done_gc_timer.schedule_after_s(_tcp_done_gc_interval);
  _nmapping_failures = 0;
  return 0;
}

void
TCPRewriter::uninitialize()
{
  clear_map(_tcp_map);
  for (int i = 0; i < _input_specs.size(); i++)
    if (_input_specs[i].kind == INPUT_SPEC_PATTERN)
      _input_specs[i].u.pattern.p->unuse();
  _input_specs.clear();
}

int
TCPRewriter::notify_pattern(Pattern *p, ErrorHandler *errh)
{
  if (!p->allow_napt())
    return errh->error("TCPRewriter cannot accept IPAddrRewriter patterns");
  return IPRw::notify_pattern(p, errh);
}

void
TCPRewriter::take_state(Element *e, ErrorHandler *errh)
{
  TCPRewriter *rw = (TCPRewriter *)e->cast("TCPRewriter");
  if (!rw) return;

  if (noutputs() != rw->noutputs()) {
    errh->warning("taking mappings from `%s', although it has %s output ports", rw->declaration().cc(), (rw->noutputs() > noutputs() ? "more" : "fewer"));
    if (noutputs() < rw->noutputs())
      errh->message("(out of range mappings will be dropped)");
  }

  _tcp_map.swap(rw->_tcp_map);

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
}

void
TCPRewriter::tcp_gc_hook(Timer *timer, void *thunk)
{
  TCPRewriter *rw = (TCPRewriter *)thunk;
  rw->clean_map(rw->_tcp_map, click_jiffies() - rw->_tcp_timeout_jiffies);
  timer->reschedule_after_s(rw->_tcp_gc_interval);
}

void
TCPRewriter::tcp_done_gc_hook(Timer *timer, void *thunk)
{
  TCPRewriter *rw = (TCPRewriter *)thunk;
  rw->clean_map_free_tracked
    (rw->_tcp_map, rw->_tcp_done, rw->_tcp_done_tail,
     click_jiffies() - rw->_tcp_done_timeout_jiffies);
  timer->reschedule_after_s(rw->_tcp_done_gc_interval);
}

TCPRewriter::TCPMapping *
TCPRewriter::apply_pattern(Pattern *pattern, int ip_p, const IPFlowID &flow,
			   int fport, int rport)
{
  assert(fport >= 0 && fport < noutputs() && rport >= 0 && rport < noutputs()
	 && ip_p == IP_PROTO_TCP);
  TCPMapping *forward = new TCPMapping(_dst_anno);
  TCPMapping *reverse = new TCPMapping(_dst_anno);

  if (forward && reverse) {
    if (!pattern)
      Mapping::make_pair(ip_p, flow, flow, fport, rport, forward, reverse);
    else if (!pattern->create_mapping(ip_p, flow, fport, rport, forward, reverse))
      goto failure;
    
    IPFlowID reverse_flow = forward->flow_id().rev();
    _tcp_map.insert(flow, forward);
    _tcp_map.insert(reverse_flow, reverse);
    return forward;
  }

 failure:
  _nmapping_failures++;
  delete forward;
  delete reverse;
  return 0;
}

void
TCPRewriter::push(int port, Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  IPFlowID flow(p);
  click_ip *iph = p->ip_header();
  click_tcp *tcph = p->tcp_header();

  // handle non-first fragments
  if (!IP_FIRSTFRAG(iph) || iph->ip_p != IP_PROTO_TCP) {
    const InputSpec &is = _input_specs[port];
    if (is.kind == INPUT_SPEC_NOCHANGE)
      output(is.u.output).push(p);
    else
      p->kill();
    return;
  }

  TCPMapping *m = static_cast<TCPMapping *>(_tcp_map.find(flow));
  
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
       m = TCPRewriter::apply_pattern(0, IP_PROTO_TCP, flow, fport, rport);
       break;
     }

     case INPUT_SPEC_PATTERN: {
       Pattern *pat = is.u.pattern.p;
       int fport = is.u.pattern.fport;
       int rport = is.u.pattern.rport;
       m = TCPRewriter::apply_pattern(pat, IP_PROTO_TCP, flow, fport, rport);
       break;
     }

     case INPUT_SPEC_MAPPER: {
       m = static_cast<TCPMapping *>(is.u.mapper->get_map(this, IP_PROTO_TCP, flow, p));
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
  if (!m->free_tracked() && (tcph->th_flags & (TH_FIN | TH_RST))
      && m->session_over())
    m->add_to_free_tracked_tail(_tcp_done, _tcp_done_tail);
}


String
TCPRewriter::dump_mappings_handler(Element *e, void *)
{
  TCPRewriter *rw = (TCPRewriter *)e;
  StringAccum tcps;
  for (Map::Iterator iter = rw->_tcp_map.first(); iter; iter++) {
    TCPMapping *m = static_cast<TCPMapping *>(iter.value());
    if (m->is_primary())
      tcps << m->s() << "\n";
  }
  return tcps.take_string();
}

String
TCPRewriter::dump_patterns_handler(Element *e, void *)
{
  TCPRewriter *rw = (TCPRewriter *)e;
  String s;
  for (int i = 0; i < rw->_input_specs.size(); i++)
    if (rw->_input_specs[i].kind == INPUT_SPEC_PATTERN)
      s += rw->_input_specs[i].u.pattern.p->s() + "\n";
  return s;
}

String
TCPRewriter::dump_nmappings_handler(Element *e, void *thunk)
{
  TCPRewriter *rw = (TCPRewriter *)e;
  if (!thunk)
    return String(rw->_tcp_map.size()) + "\n";
  else
    return String(rw->_nmapping_failures) + "\n";
}

void
TCPRewriter::add_handlers()
{
  add_read_handler("mappings", dump_mappings_handler, (void *)0);
  add_read_handler("nmappings", dump_nmappings_handler, (void *)0);
  add_read_handler("mapping_failures", dump_nmappings_handler, (void *)1);
  add_read_handler("patterns", dump_patterns_handler, (void *)0);
}

int
TCPRewriter::llrpc(unsigned command, void *data)
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
    TCPMapping *m = get_mapping(IP_PROTO_TCP, flowid);
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
EXPORT_ELEMENT(TCPRewriter)
