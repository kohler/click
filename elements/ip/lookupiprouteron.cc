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
  _t = new IPTableRON();
}

LookupIPRouteRON::~LookupIPRouteRON()
{
  MOD_DEC_USE_COUNT;
  delete(_t);
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
  click_tcp *tcph;
  int match_type; 
  TableEntry *match = NULL;
  TableEntry *new_entry = NULL;

  tcph = reinterpret_cast<click_tcp *>(p->transport_header());

  match_type = _t->lookup(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			  ntohs(tcph->th_sport), ntohs(tcph->th_dport),
			  &match);
  printf("TCP SYN match type: %d\n", match_type);

  // some kind of match was found
  switch(match_type) {
  case EXACT_ACTIVE_RECENT: 
  case EXACT_ACTIVE_OLD:
  case EXACT_INACTIVE_RECENT:
    match->saw_forward_packet();
    output(match->outgoing_port).push(p);
    return;

  case SIMILAR_ACTIVE_RECENT:
  case SIMILAR_INACTIVE_RECENT:
    // Add new entry to table
    new_entry = _t->add(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			ntohs(tcph->th_sport), ntohs(tcph->th_dport),
			click_jiffies());
    new_entry->outgoing_port = match->outgoing_port;
    new_entry->saw_forward_packet();
    output(new_entry->outgoing_port).push(p);
    return;

  case SIMILAR_PENDING:
  case SIMILAR_WAITING:
    // Add new entry to table
    new_entry = _t->add(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			ntohs(tcph->th_sport), ntohs(tcph->th_dport),
			click_jiffies());
    new_entry->saw_forward_packet();
    //wait until pending probes have returned
    new_entry->add_waiting(p);
    return;

    //case EXACT_PENDING: // YIPAL DEBUGGING
    

    //  case EXACT_WAITING:
    //wait until pending probes have returned
    match->add_waiting(p);
    return;

  case EXACT_WAITING:
  case EXACT_PENDING:

  case EXACT_INACTIVE_OLD:
  case SIMILAR_ACTIVE_OLD: // as good as no match
  case NOMATCH: 
    printf("no match (SENDING PROBE)\n");
    // Start new probe
    // Add new entry to table (Assumes this is a SYN)
    new_entry = _t->add(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			ntohs(tcph->th_sport), ntohs(tcph->th_dport),
			click_jiffies());
    new_entry->saw_forward_packet();
    new_entry->outstanding_syns++;
    // send probes along all routes
    duplicate_pkt(p);

    break;
  }
}
void LookupIPRouteRON::push_forward_fin(Packet *p) 
{
  click_tcp *tcph;
  int match_type; 
  TableEntry *match = NULL;
  TableEntry *new_entry = NULL;

  tcph = reinterpret_cast<click_tcp *>(p->transport_header());

  match_type = _t->lookup(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			  ntohs(tcph->th_sport), ntohs(tcph->th_dport),
			  &match);
  printf("TCP FIN packet match type: %d\n", match_type);

  // some kind of match was found
  switch(match_type) {
  case EXACT_PENDING:
    // get rid of all waiting packets
    match->saw_forward_packet();
    match->clear_waiting();
    match->forw_alive = 0; // forward flow is over
    // YIPAL: If both flows are closed then
    //        notify SIMILAR flows with waiting packets
    // YIPAL: Perhaps this should send RST instead of FIN
    duplicate_pkt(p);
    return;

  case EXACT_WAITING:
    // get rid of all waiting packets
    //match->clear_waiting();
    match->add_waiting(p);
    match->forw_alive = 0; // forward flow is over
    return;

  case EXACT_ACTIVE_RECENT: 
  case EXACT_ACTIVE_OLD:
  case EXACT_INACTIVE_RECENT: // leftover packets ?
  case EXACT_INACTIVE_OLD:
    match->saw_forward_packet();
    match->clear_waiting(); // there shouldn't be any waiting packets anyways
    match->forw_alive = 0; // forward flow is over
    output(match->outgoing_port).push(p);
    return;

  case SIMILAR_ACTIVE_RECENT: // similar matches can be considered 
  case SIMILAR_PENDING:       // not maching for "fin" packets
  case SIMILAR_WAITING:
  case SIMILAR_ACTIVE_OLD:
  case SIMILAR_INACTIVE_RECENT:
  case NOMATCH:
    // YIPAL: perhaps there is something more intelligent we can do with these
    printf("killed FIN pkt b/c no exact matches\n");
    p->kill();  
    return;
  }
}
void LookupIPRouteRON::push_forward_rst(Packet *p) 
{
  click_tcp *tcph;
  int match_type; 
  TableEntry *match = NULL;
  TableEntry *new_entry = NULL;

  tcph = reinterpret_cast<click_tcp *>(p->transport_header());

  match_type = _t->lookup(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			  ntohs(tcph->th_sport), ntohs(tcph->th_dport),
			  &match);
  printf("TCP RST packet match type: %d\n", match_type);

  // some kind of match was found
  switch(match_type) {
  case EXACT_PENDING:    
    // get rid of all waiting packets
    match->saw_forward_packet();
    match->clear_waiting();
    match->forw_alive = 0; // forward flow is over
    match->rev_alive = 0; //  reverse flow is over
    // YIPAL: If both flows are closed then
    //        notify SIMILAR flows with waiting packets
    duplicate_pkt(p);
    return;

  case EXACT_WAITING:
    // get rid of all waiting packets
    match->clear_waiting();
    match->forw_alive = 0; // forward flow is over
    match->rev_alive = 0; //  reverse flow is over
    return;

  case EXACT_ACTIVE_RECENT: 
  case EXACT_ACTIVE_OLD:
  case EXACT_INACTIVE_RECENT:  // left over packets? 
  case EXACT_INACTIVE_OLD:
    match->saw_forward_packet();
    match->clear_waiting();// there shouldn't be any waiting ones anyway.
    match->forw_alive = 0; // forward flow is over
    match->rev_alive = 0; //  reverse flow is over
    output(match->outgoing_port).push(p);
    return;

  case SIMILAR_ACTIVE_RECENT: // similar matches can be considered 
  case SIMILAR_INACTIVE_RECENT: // similar matches can be considered 
  case SIMILAR_PENDING:       // not maching for "rst" packets
  case SIMILAR_WAITING:
  case SIMILAR_ACTIVE_OLD: 
  case NOMATCH:
    // YIPAL: perhaps there is something more intelligent we can do with these
    printf("killed RST pkt b/c no exact matches\n");
    p->kill();  
    return;
  }
}
void LookupIPRouteRON::push_forward_normal(Packet *p) 
{
  // what to do in the case of a forward direction syn.
  click_tcp *tcph;
  int match_type; 
  TableEntry *match = NULL;
  TableEntry *new_entry = NULL;

  tcph = reinterpret_cast<click_tcp *>(p->transport_header());

  match_type = _t->lookup(IPAddress(p->ip_header()->ip_src), p->dst_ip_anno(),
			  ntohs(tcph->th_sport), ntohs(tcph->th_dport),
			  &match);
  printf("TCP normal packet match type: %d\n", match_type);

  // some kind of match was found
  switch(match_type) {
  case EXACT_ACTIVE_RECENT: 
  case EXACT_ACTIVE_OLD:
  case EXACT_INACTIVE_RECENT:  // 
  case EXACT_INACTIVE_OLD:
    match->saw_forward_packet();
    output(match->outgoing_port).push(p);
    return;

  case EXACT_PENDING:
  case EXACT_WAITING:
    //wait until pending probes have returned
    match->add_waiting(p);
    return;

  case SIMILAR_ACTIVE_RECENT: // similar matches can be considered 
  case SIMILAR_PENDING:       // not maching for "normal" packets
  case SIMILAR_WAITING:
  case SIMILAR_ACTIVE_OLD: 
  case SIMILAR_INACTIVE_RECENT:
  case NOMATCH:
    // YIPAL: perhaps there is something more intelligent we can do with these
    printf("killed normal pkt b/c no exact matches\n");
    p->kill();  
    return;
  }
}

void LookupIPRouteRON::push_forward_packet(Packet *p) 
{
  click_tcp *tcph;

  // if non-TCP just forward direct 
  // (YIPAL: perhaps this could be more intelligent)
  if (p->ip_header()->ip_p != IP_PROTO_TCP) {
    printf("non-TCP proto(%d)\n", p->ip_header()->ip_p);
    output(1).push(p);
    return;
  }

  // Switch on TCP packet type
  tcph = reinterpret_cast<click_tcp *>(p->transport_header());
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

void LookupIPRouteRON::push_reverse_synack(int inport, Packet *p) 
{
  click_tcp *tcph;
  int match_type; 
  TableEntry *match = NULL;
  tcph = reinterpret_cast<click_tcp *>(p->transport_header());

  match_type = _t->lookup(p->dst_ip_anno(),IPAddress(p->ip_header()->ip_src),
			  ntohs(tcph->th_dport), ntohs(tcph->th_sport),
			  &match);
  printf("Rev TCP SYN-ACK packet match type: %d\n", match_type);

  switch(match_type) {
  case EXACT_PENDING:
    // this is the FIRST SYN-ACK to return
    match->outgoing_port = inport;
    match->outstanding_syns = 0;
    match->saw_reply_packet();
    //match->push_waiting( output(inport) );
    _t->send_similar_waiting(p->dst_ip_anno(), output(inport));
    output(0).push(p);
    // send similar entries which are waiting
    return;


  case EXACT_ACTIVE_RECENT: 
  case EXACT_ACTIVE_OLD:
  case EXACT_INACTIVE_RECENT: 
  case EXACT_INACTIVE_OLD:
    // this is not the first SYN-ACK to return

    if (inport != match->outgoing_port){
      // this is one of the Probe SYN-ACKs but we already go the first one
      // YIPAL: send a RST to the server
      p->kill();
      
    } else {
      // this is a random packet in the right flow
      match->saw_reply_packet();
      output(0).push(p);
    }
    return;    

  case EXACT_WAITING:
    // YIPAL: this would be an unusual event. 

  case SIMILAR_ACTIVE_RECENT: 
  case SIMILAR_INACTIVE_RECENT: 
  case SIMILAR_PENDING:       
  case SIMILAR_WAITING:
  case SIMILAR_ACTIVE_OLD: 
  case NOMATCH:
    // no exact matches, dont know what to do with such a SYN-ACK
    p->kill();
    return;

  }
}
void LookupIPRouteRON::push_reverse_fin(int inport, Packet *p) 
{
  click_tcp *tcph;
  int match_type; 
  TableEntry *match = NULL;
  tcph = reinterpret_cast<click_tcp *>(p->transport_header());

  match_type = _t->lookup(p->dst_ip_anno(),IPAddress(p->ip_header()->ip_src),
			  ntohs(tcph->th_dport), ntohs(tcph->th_sport),
			  &match);
  printf("Rev TCP FIN packet match type: %d\n", match_type);

  switch(match_type) {
  case EXACT_ACTIVE_RECENT: 
  case EXACT_ACTIVE_OLD:
  case EXACT_INACTIVE_RECENT: 
  case EXACT_INACTIVE_OLD:
  case EXACT_WAITING:
  case EXACT_PENDING:

    match->saw_reply_packet();
    match->rev_alive = 0;
    // YIPAL: If both flows are closed then
    //        notify SIMILAR flows with waiting packets(tell them to probe)
    output(0).push(p);
    return;

  case SIMILAR_ACTIVE_RECENT: 
  case SIMILAR_INACTIVE_RECENT: 
  case SIMILAR_PENDING:       
  case SIMILAR_WAITING:
  case SIMILAR_ACTIVE_OLD: 
  case NOMATCH:
    // no exact matches, dont know what to do with such a SYN-ACK
    p->kill();
    return;
  }
}
void LookupIPRouteRON::push_reverse_rst(int inport, Packet *p) 
{
  click_tcp *tcph;
  int match_type; 
  TableEntry *match = NULL;
  tcph = reinterpret_cast<click_tcp *>(p->transport_header());

  match_type = _t->lookup(p->dst_ip_anno(),IPAddress(p->ip_header()->ip_src),
			  ntohs(tcph->th_dport), ntohs(tcph->th_sport),
			  &match);
  printf("Rev TCP RST packet match type: %d\n", match_type);

  switch(match_type) {
  case EXACT_PENDING:
  case EXACT_WAITING:
  case EXACT_ACTIVE_RECENT: 
  case EXACT_ACTIVE_OLD:
  case EXACT_INACTIVE_RECENT: 
  case EXACT_INACTIVE_OLD:
    match->clear_waiting();
    match->forw_alive = 0; // forward flow is over
    match->rev_alive = 0; //  reverse flow is over
    output(0).push(p);
    return;

  case SIMILAR_ACTIVE_RECENT: 
  case SIMILAR_PENDING:       
  case SIMILAR_WAITING:
  case SIMILAR_ACTIVE_OLD: 
  case NOMATCH:
    p->kill();
    return;
  }
}
void LookupIPRouteRON::push_reverse_normal(int inport, Packet *p) 
{
  click_tcp *tcph;
  int match_type; 
  TableEntry *match = NULL;
  tcph = reinterpret_cast<click_tcp *>(p->transport_header());

  match_type = _t->lookup(p->dst_ip_anno(),IPAddress(p->ip_header()->ip_src),
			  ntohs(tcph->th_dport), ntohs(tcph->th_sport),
			  &match);
  printf("Rev TCP normal packet match type: %d\n", match_type);

  switch(match_type) {
  case EXACT_PENDING:
  case EXACT_WAITING:
    // YIPAL: dont know what to do with this
    p->kill();
    return;

  case EXACT_ACTIVE_RECENT: 
  case EXACT_ACTIVE_OLD:
    match->saw_reply_packet();
    output(0).push(p);
    return;

  case SIMILAR_ACTIVE_RECENT: 
  case SIMILAR_PENDING:       
  case SIMILAR_WAITING:
  case SIMILAR_ACTIVE_OLD: 
  case NOMATCH:
    // no exact matches, dont know what to do with such a SYN-ACK
    p->kill();
    return;
  }
}


void LookupIPRouteRON::push_reverse_packet(int inport, Packet *p) 
{
  click_tcp *tcph;

  // if non-TCP just forward direct 
  // (YIPAL: perhaps this could be more intelligent)
  if (p->ip_header()->ip_p != IP_PROTO_TCP) {
    printf("non-TCP proto(%d)\n", p->ip_header()->ip_p);
    output(0).push(p);
    return;
  }

  // Switch on TCP packet type
  tcph = reinterpret_cast<click_tcp *>(p->transport_header());
  if ((tcph->th_flags & TH_SYN) && (tcph->th_flags & TH_ACK)) {
    push_reverse_synack(inport, p);
  } else if (tcph->th_flags & TH_FIN) {
    push_reverse_fin(inport, p);
  } else if (tcph->th_flags & TH_RST) {
    push_reverse_rst(inport, p);
  } else {
    push_reverse_normal(inport, p);
  }
}

void
LookupIPRouteRON::push(int inport, Packet *p)
{
  //click_tcp *tcph;
  //TableEntry *e;
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

  _t->print();  
  printf("\n");
}

void LookupIPRouteRON::expire_hook(Timer *, void *thunk) 
{
  Packet *p;
  Vector<TableEntry*> syn_waiting;
  LookupIPRouteRON *rt = (LookupIPRouteRON *) thunk;
  
  // push SYNS that have been waiting too long
  rt->_t->get_waiting_syns(&syn_waiting);
  
  for(int i=0; i<syn_waiting.size(); i++) {
    syn_waiting[i]->get_first_waiting();
  }

  
}

// ------ IPTableRON methods -------
LookupIPRouteRON::IPTableRON::IPTableRON() {
}

LookupIPRouteRON::IPTableRON::~IPTableRON() {
}

void
LookupIPRouteRON::IPTableRON::get_waiting_syns(Vector<TableEntry*> *t)
{
  for(int i=0; i<_v.size(); i++) {

    // add 
    if ( // _v[i].is_pending() && 
        _v[i].get_age() > WAIT_TIMEOUT &&
	_v[i].is_waiting() &&
	_v[i].is_valid() ) {
      // YIPAL: Check if SYN pkt
     t->push_back(&_v[i]);
    }

  }
} 
  

void
LookupIPRouteRON::IPTableRON::send_similar_waiting(IPAddress dst, 
						   const Element::Port p) {
  // Looks in table for pkts waiting for <dst>. Pushes such packets.
  TableEntry *match = NULL;

  // go through table, looking for dst matches
  for (int i = 0; i < _v.size(); i++){
    if ( dst == _v[i].dst) {
      _v[i].push_all_waiting(p);
    }
  }   
}

int
LookupIPRouteRON::IPTableRON::lookup(IPAddress src, IPAddress dst,
				     unsigned short sport, unsigned short dport,
				     struct LookupIPRouteRON::TableEntry **entry){
  TableEntry *exact   = NULL;
  TableEntry *similar = NULL;
  int match_type;

  // find a valid match
  for (int i = 0; i < _v.size(); i++){
    
    if ( (src == _v[i].src) && (dst == _v[i].dst) && 
	 (sport == _v[i].sport) && (dport == _v[i].dport) &&
	 (_v[i].is_valid()) ) {
      // exact match is found
      
      exact = &_v[i];
      *entry = exact;

      match_type = exact->get_state();

      switch(match_type) {
      case TableEntry::ACTIVE_RECENT:
	return EXACT_ACTIVE_RECENT;
      case TableEntry::INACTIVE_RECENT:
	return EXACT_INACTIVE_RECENT;
      case TableEntry::PENDING:
	return EXACT_PENDING;
      case TableEntry::WAITING:
	return EXACT_WAITING;
      case TableEntry::ACTIVE_OLD:
	return EXACT_ACTIVE_OLD;
      case TableEntry::INACTIVE_OLD:
	return EXACT_INACTIVE_OLD;
      case TableEntry::INVALID:
	return NOMATCH;
      }

    } else if((dst == _v[i].dst) && /*(dport == _v[i].dport) &&*/
	      (_v[i].is_valid())) {
      // similar match was found
      if (!similar)
	similar = &_v[i];

      else {
	// save this similar match if it's more informative.
	if (_v[i].get_state() < similar->get_state())
	  similar = &_v[i];
      }
    }
  }

  if (similar) {
    *entry = similar;
    switch(similar->get_state()) {
    case TableEntry::ACTIVE_RECENT :
      return SIMILAR_ACTIVE_RECENT;
    case TableEntry::INACTIVE_RECENT:
      return SIMILAR_INACTIVE_RECENT;
    case TableEntry::ACTIVE_OLD:
      return SIMILAR_ACTIVE_OLD;
    case TableEntry::PENDING:
      return SIMILAR_PENDING;
    case TableEntry::WAITING:
      return SIMILAR_WAITING;
    }
  }
  // no match found
  *entry = 0;
  return NOMATCH;
}

LookupIPRouteRON::TableEntry*
LookupIPRouteRON::IPTableRON::add(IPAddress src, IPAddress dst, 
		unsigned short sport, unsigned short dport, 
		//unsigned outgoing_port,
		//unsigned oldest_unanswered, unsigned last_reply, 
		unsigned probe_time) {
		//bool forw_alive, bool rev_alive, unsigned outstanding_syns){

  TableEntry e;
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
  e.clear_waiting();

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
LookupIPRouteRON::IPTableRON::del(IPAddress dst){
  for (int i = 0; i < _v.size(); i++)
    if (_v[i].dst == dst) {
      _v[i].dst = IPAddress(1);
      _v[i].forw_alive = 0;
      _v[i].rev_alive  = 0;
      _v[i].invalidate(); // this "deletes" the entry
      return;
    }
  
}

void
LookupIPRouteRON::IPTableRON::print() {
  printf("  Table contents size(%d):\n", _v.size());
  for (int i = 0; i < _v.size(); i++) {
    if (_v[i].is_valid()) {
      //printf( "    [%d] %02x%02x%02x%02x(%u) -> %02x%02x%02x%02x(%u) ",
      printf( "    [%d] %d.%d.%d.%d(%u) -> %d.%d.%d.%d(%u) \t",
	      _v[i].get_state(),
	      _v[i].src.data()[0],_v[i].src.data()[1], 
	      _v[i].src.data()[2],_v[i].src.data()[3],
	      _v[i].sport,

	      _v[i].dst.data()[0],_v[i].dst.data()[1], 
	      _v[i].dst.data()[2],_v[i].dst.data()[3],
	      _v[i].dport);
      printf("outport(%d) pending(%d) waiting(%d) \tage(%u) FR = %d%d\n", 
	     _v[i].outgoing_port, _v[i].outstanding_syns,
	     _v[i].waiting.size(), _v[i].get_age(), 	     
	     _v[i].forw_alive, _v[i].rev_alive);

      if (_v[i].get_state() == TableEntry::INVALID) 
	exit(0);
    }
  }
}



// generate Vector template instance
#include <click/vector.cc>
// must always generate the whole instance! LookupIPRoute demands it
template class Vector<LookupIPRouteRON::TableEntry>;


EXPORT_ELEMENT(LookupIPRouteRON)







