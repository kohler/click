/*
 * lookupiprouteron.{cc,hh} -- element looks up next-hop address for RON
 * Alexander Yip
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
#include "lookupiprouteron.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/click_tcp.h>

LookupIPRouteRON::LookupIPRouteRON() 
  : _expire_timer(expire_hook, (void *) this)
{
  MOD_INC_USE_COUNT;
  add_input();
  _flow_table = new FlowTable();
  _dst_table  = new DstTable();
}

LookupIPRouteRON::~LookupIPRouteRON()
{
  MOD_DEC_USE_COUNT;
  delete(_flow_table);
  delete(_dst_table);
  _expire_timer.unschedule();
}

LookupIPRouteRON *
LookupIPRouteRON::clone() const
{
  return new LookupIPRouteRON;
}

int
LookupIPRouteRON::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int n = noutputs();
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpUnsigned, "number of ports", &n,
		  0) < 0)
    return -1;
  if (n < 1)
    return errh->error("number of ports must be at least 1");

  set_noutputs(n+1);
  set_ninputs(n+1);
  return 0;
}

int
LookupIPRouteRON::initialize(ErrorHandler *)
{
  /*
  _last_addr = IPAddress();
#ifdef IP_RT_CACHE2
  _last_addr2 = _last_addr;
#endif
  */
  _expire_timer.initialize(this);
  //_expire_timer.schedule_after_ms(EXPIRE_TIMEOUT_MS);

  return 0;
}

void LookupIPRouteRON::duplicate_pkt(Packet *p) {

  int n = noutputs();
  for (int i =1; i < n - 1; i++)
    if (Packet *q = p->clone())
      output(i).push(q);
  output(n - 1).push(p);
}

void LookupIPRouteRON::push_forward_syn(Packet *p) 
{

  // what to do in the case of a forward direction syn.
  const click_tcp *tcph;
  FlowTableEntry *match = NULL;
  FlowTableEntry *new_entry = NULL;
  DstTableEntry  *dst_match = NULL;

  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());

  match = _flow_table->lookup(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			      ntohs(tcph->th_sport), ntohs(tcph->th_dport));
  
  printf ("FOR TCP SYN\n");
  
  if (match) {
    // Match found
    match->saw_forward_packet();
    
    if (match->is_pending()){
      printf("FLOW match(pending), send PROBE\n");
      match->outstanding_syns++;
      duplicate_pkt(p);
    } else {
      printf("FLOW match, FORW\n");
      output(match->outgoing_port).push(p);
    }    
  } else {
    // NO match, Look into Dst Table
    dst_match = _dst_table->lookup(p->dst_ip_anno());


    // Add new entry to Flow Table
    new_entry = _flow_table->add(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
				 ntohs(tcph->th_sport), ntohs(tcph->th_dport),
				 click_jiffies());
    new_entry->saw_forward_packet();
    
    if (dst_match){
      new_entry->outgoing_port = dst_match->outgoing_port;
      output(new_entry->outgoing_port).push(p);
      printf("DST match, FORW\n");
    } else {
      printf("DST nomatch, send PROBE\n");
      new_entry->outgoing_port = 0;
      new_entry->outstanding_syns++;
      duplicate_pkt(p);
    }
  }
  
  return;
}
void LookupIPRouteRON::push_forward_fin(Packet *p) 
{
  const click_tcp *tcph;
  FlowTableEntry *match = NULL;

  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
  
  printf("FOR TCP FIN\n");

  match = _flow_table->lookup(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			      ntohs(tcph->th_sport), ntohs(tcph->th_dport));
  if (match) {
    if (!match->is_pending()) {
      printf(" found non-pending match, ending forward connection\n");
      match->saw_forward_packet();
      match->forw_alive = 0; // forward flow is over
      output(match->outgoing_port).push(p);
      return;
    } 
  }
  printf(" could not find non-pending match, killing pkt.\n");
  p->kill();
  return;
}

void LookupIPRouteRON::push_forward_rst(Packet *p) 
{
  const click_tcp *tcph;
  FlowTableEntry *match = NULL;

  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
  printf("FOR TCP RST\n");

  match = _flow_table->lookup(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			      ntohs(tcph->th_sport), ntohs(tcph->th_dport));
  
  if (match) {
    if (!match->is_pending()) {
      printf("found match, non-pending, forwarding...\n");
      match->saw_forward_packet();
      match->forw_alive = 0; // forward & reverse flows are over
      match->rev_alive = 0; 
      output(match->outgoing_port).push(p);
      return;
    }
  }
  
  printf("could not find non-pending match. Killing packet\n");
  p->kill();
}

void LookupIPRouteRON::push_forward_normal(Packet *p) 
{
  // what to do in the case of a forward direction syn.
  const click_tcp *tcph;
  FlowTableEntry *match = NULL;

  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());

  printf("FOR TCP normal pkt\n");

  match = _flow_table->lookup(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			      ntohs(tcph->th_sport), ntohs(tcph->th_dport));
  
  if (match) {
    if (!match->is_pending()) {
      printf("found match, not pending, forwarding...\n");
      match->saw_forward_packet();
      output(match->outgoing_port).push(p);
      return;
    }
  }
  
  printf("could not find non-pending match. Killing packet\n");
  p->kill();

}

void LookupIPRouteRON::push_forward_packet(Packet *p) 
{
  const click_tcp *tcph;

  // if non-TCP just forward direct 
  // (YIPAL: perhaps this could be more intelligent)
  if (p->ip_header()->ip_p != IP_PROTO_TCP) {
    printf("non-TCP proto(%d)\n", p->ip_header()->ip_p);
    output(1).push(p);
    return;
  }

  // Switch on TCP packet type
  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
  if (tcph->th_flags & TH_SYN) {
    push_forward_syn(p);
  } else if (tcph->th_flags & TH_FIN) {
    push_forward_fin(p);
  } else if (tcph->th_flags & TH_RST) {
    push_forward_rst(p);
  } else {
    push_forward_normal(p);
  }
}

void LookupIPRouteRON::push_reverse_synack(unsigned inport, Packet *p) 
{
  const click_tcp *tcph;
  FlowTableEntry *match = NULL;
  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());

  match = _flow_table->lookup(p->dst_ip_anno(),IPAddress(p->ip_header()->ip_src),
			      ntohs(tcph->th_dport), ntohs(tcph->th_sport));
  
  printf ("REV TCP SYN-ACK inport(%d)\n", inport);
  
  if (match) { 
    match->saw_reply_packet();
    
    if (match->is_pending()) {
      printf("FLOW match(pending), setting up flow, FORW\n");
      _dst_table->insert(IPAddress(p->ip_header()->ip_src), inport); // save to dst_table
      match->outgoing_port = inport;
      match->outstanding_syns = 0;
      output(0).push(p);
    } else {
      // FLOW not pending
      if (inport == match->outgoing_port){
	printf("Correct return port, forwarding reverse SYN-ACK\n");
	output(0).push(p);
      } else {
	printf("Incorrect return port, killing SYN-ACK\n");
	p->kill();
      }
    }
  } else {
    printf("FLOW no match, killing SYN-ACK\n");
    p->kill();
  }

}
void LookupIPRouteRON::push_reverse_fin(Packet *p) 
{
  const click_tcp *tcph;
  FlowTableEntry *match = NULL;
  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());

  match = _flow_table->lookup(p->dst_ip_anno(),IPAddress(p->ip_header()->ip_src),
			      ntohs(tcph->th_dport), ntohs(tcph->th_sport));
  
  printf("REV TCP FIN\n");

  if (match) {
    if (!match->is_pending()) {
      printf(" found match, not pending, ending reverse direction\n");
      match->saw_reply_packet();
      match->rev_alive = 0;
      output(0).push(p);
      return;
    }
  }

  printf(" could not find non-pending match. Killing pkt.\n");
  p->kill();
}
void LookupIPRouteRON::push_reverse_rst(Packet *p) 
{
  const click_tcp *tcph;
  FlowTableEntry *match = NULL;
  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
  match = _flow_table->lookup(p->dst_ip_anno(),IPAddress(p->ip_header()->ip_src),
			      ntohs(tcph->th_dport), ntohs(tcph->th_sport));
  
  printf("REV TCP RST\n");

  if (match) {
    if (!match->is_pending()) {
      printf(" found match, not pending, ending reverse direction\n");
      match->saw_reply_packet();
      match->forw_alive = 0;
      match->rev_alive = 0;
      output(0).push(p);
      return;
    }
  }

  printf(" could not find non-pending match. Killing pkt.\n");
  p->kill();
}
void LookupIPRouteRON::push_reverse_normal(Packet *p) 
{
  const click_tcp *tcph;
  FlowTableEntry *match = NULL;
  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
 
  match = _flow_table->lookup(p->dst_ip_anno(),IPAddress(p->ip_header()->ip_src),
			      ntohs(tcph->th_dport), ntohs(tcph->th_sport));

  printf("REV TCP normal pkt\n");

  if (match) {
    if (!match->is_pending()) {
      printf("found match, not pending, forwarding...\n");
      match->saw_reply_packet();
      output(0).push(p);
      return;
    }
  }
  
  printf("could not find non-pending match. Killing packet\n");
  p->kill();
}


void LookupIPRouteRON::push_reverse_packet(int inport, Packet *p) 
{
  const click_tcp *tcph;

  // if non-TCP just forward direct 
  // (YIPAL: perhaps this could be more intelligent)
  if (p->ip_header()->ip_p != IP_PROTO_TCP) {
    printf("non-TCP proto(%d)\n", p->ip_header()->ip_p);
    output(0).push(p);
    return;
  }

  // Switch on TCP packet type
  tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
  if ((tcph->th_flags & TH_SYN) && (tcph->th_flags & TH_ACK)) {
    push_reverse_synack(inport, p);
  } else if (tcph->th_flags & TH_FIN) {
    push_reverse_fin(p);
  } else if (tcph->th_flags & TH_RST) {
    push_reverse_rst(p);
  } else {
    push_reverse_normal(p);
  }
}

void
LookupIPRouteRON::push(int inport, Packet *p)
{
  //click_tcp *tcph;
  //FlowTableEntry *e;
  //int matchState;

  if (inport == 0) {
    push_forward_packet(p);
  } else {
    push_reverse_packet(inport, p);
  }

  /*
  //// ---- TEST CODE -----  
  printf("adding address to table(%u): \n", click_jiffies());
  tcph = reinterpret_cast<click_tcp *>(p->transport_header());
  
  _t->add(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(), 
	  ntohs(tcph->th_sport), ntohs(tcph->th_dport), 10, 
	  11, 12, click_jiffies(), 1, 1, 1);

  _t->print();
  //_t->del(IPAddress("1.1.1.1"));
  //_t->print();  
  matchState = _t->lookup(IPAddress("18.7.0.12"), IPAddress("18.239.0.139"), 
			  80, 1410, &e);
  
  if (e)
    printf("match type(%d) ports %u -> %u\n",matchState, e->sport, e->dport);
  else
    printf("no match\n");
  */

  _flow_table->print();  
  _dst_table->print();
  printf("\n");
}

void LookupIPRouteRON::expire_hook(Timer *, void *thunk) 
{
  /*
  Packet *p;
  Vector<FlowTableEntry*> syn_waiting;
  LookupIPRouteRON *rt = (LookupIPRouteRON *) thunk;
  */
}

static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
LookupIPRouteRON::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}



// ------ FlowTable methods -------
LookupIPRouteRON::FlowTable::FlowTable() {
}

LookupIPRouteRON::FlowTable::~FlowTable() {
}

LookupIPRouteRON::FlowTableEntry *
LookupIPRouteRON::FlowTable::lookup(IPAddress src, IPAddress dst,
				    unsigned short sport, unsigned short dport){

  printf("LOOKUP: %d.%d.%d.%d(%d) -> %d.%d.%d.%d(%d)\n",
	 src.data()[0], src.data()[1], src.data()[2], src.data()[3], sport,
	 dst.data()[0], dst.data()[1], dst.data()[2], dst.data()[3], dport);
  // find a valid match
  for (int i = 0; i < _v.size(); i++){
    
    if ( (src == _v[i].src) && (dst == _v[i].dst) && 
	 (sport == _v[i].sport) && (dport == _v[i].dport)) {

      if (_v[i].is_valid()) {
	// exact match is found
	return &_v[i];
      } else
	printf("TABLE: invalid match found\n"); 
      
    }
  }
  // no match found
  return 0;
}

LookupIPRouteRON::FlowTableEntry*
LookupIPRouteRON::FlowTable::add(IPAddress src, IPAddress dst, 
				 unsigned short sport, unsigned short dport, 
				 unsigned probe_time) {

  FlowTableEntry e;
  e.src = src;
  e.dst = dst;
  e.sport = sport;
  e.dport = dport;
  e.outgoing_port = 0;
  e.oldest_unanswered = 0;
  e.last_reply = 0; 
  e.forw_alive = 1;
  e.rev_alive = 1;
  e.outstanding_syns = 0;
  e.probe_time = probe_time;
  //e.clear_waiting();

  // replace duplicate entry first
  for (int i = 0; i < _v.size(); i++)
    if ((src == _v[i].src) && (dst == _v[i].dst) && 
	(sport == _v[i].sport) && (dport == _v[i].dport)) {
      _v[i] = e;
      printf("  replacing existing entry in table\n");
      return &_v[i];
    }

  // replace invalid entries
  for (int i = 0; i < _v.size(); i++)
    if (!_v[i].is_valid() || (!_v[i].is_active() && _v[i].is_old()) ) {
      _v[i] = e;
      printf("  replacing invalid entry in table\n");
      return &_v[i];
    }

  // just push new entry onto back of table.
  printf("  adding new entry to table\n");
  _v.push_back(e);
  return &_v[_v.size()-1];

}

void 
LookupIPRouteRON::FlowTable::del(IPAddress src, IPAddress dst, 
				 unsigned short sport, unsigned short dport){
  // find a match
  for (int i = 0; i < _v.size(); i++){
    
    if ( (src == _v[i].src) && (dst == _v[i].dst) && 
	 (sport == _v[i].sport) && (dport == _v[i].dport)) {
      // exact match is found
      _v[i].dst = IPAddress(1);
      _v[i].forw_alive = 0;
      _v[i].rev_alive  = 0;
      _v[i].invalidate(); // this "deletes" the entry
      return;
    }
  }
}

void
LookupIPRouteRON::FlowTable::print() {
  printf("  Table contents size(%d):\n", _v.size());
  for (int i = 0; i < _v.size(); i++) {
    if (_v[i].is_valid()) {
      //printf( "    [%d] %02x%02x%02x%02x(%u) -> %02x%02x%02x%02x(%u) ",
      printf( "    [%d] %d.%d.%d.%d(%u) -> %d.%d.%d.%d(%u) \t",
	      //_v[i].get_state(),
	      0,
	      _v[i].src.data()[0],_v[i].src.data()[1], 
	      _v[i].src.data()[2],_v[i].src.data()[3],
	      _v[i].sport,

	      _v[i].dst.data()[0],_v[i].dst.data()[1], 
	      _v[i].dst.data()[2],_v[i].dst.data()[3],
	      _v[i].dport);
      printf("outport(%d) pending(%d) waiting(%d) \tage(%u) FR = %d%d\n", 
	     _v[i].outgoing_port, _v[i].outstanding_syns,
	     //_v[i].waiting.size(), 
	     0,
	     _v[i].get_age(), 	     
	     _v[i].forw_alive, _v[i].rev_alive);

      //if (_v[i].get_state() == FlowTableEntry::INVALID) exit(0);
    }
  }
}

LookupIPRouteRON::DstTable::DstTable() {
}
LookupIPRouteRON::DstTable::~DstTable() {
}

LookupIPRouteRON::DstTableEntry*
LookupIPRouteRON::DstTable::lookup(IPAddress dst) {
  for(int i=0; i<_v.size(); i++) {
    if (_v[i].dst == dst && 
	_v[i].is_recent() &&
	_v[i].is_valid() ) {
      return &_v[i];
    }
  }
  return 0;
}

void
LookupIPRouteRON::DstTable::insert(IPAddress dst, unsigned short assigned_port) {
  int replaceme = -1;
  
  DstTableEntry e;

  for(int i=0; i<_v.size(); i++) {
    if (_v[i].dst == dst) {
      _v[i].outgoing_port = assigned_port;
      _v[i].probe_time = click_jiffies();
      return;
    }
    if (!_v[i].is_recent()) {
      replaceme = i;
    }
  }
  
  if (replaceme != -1) {
      _v[replaceme].dst = dst;
      _v[replaceme].outgoing_port = assigned_port;
      _v[replaceme].probe_time = click_jiffies();
      return;
  }
  
  e.dst = dst;
  e.outgoing_port = assigned_port;
  e.probe_time = click_jiffies();
  _v.push_back(e);
  return;
}

void
LookupIPRouteRON::DstTable::print() {
  printf("DST Table Contents(%d)\n", _v.size());
  for(int i=0; i<_v.size(); i++) {
    printf("  %d.%d.%d.%d \t port(%d) valid(%d) recent(%d)\n", 
	   _v[i].dst.data()[0], _v[i].dst.data()[1], 
	   _v[i].dst.data()[2], _v[i].dst.data()[3], 
	   _v[i].outgoing_port, _v[i].is_valid(), _v[i].is_recent());
  }
}


// generate Vector template instance
#include <click/vector.cc>
// must always generate the whole instance! LookupIPRoute demands it
template class Vector<LookupIPRouteRON::FlowTableEntry>;


EXPORT_ELEMENT(LookupIPRouteRON)
