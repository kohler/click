/*
 * policyprobe.{cc,hh} -- Implements a policy for probing paths
 * Alexander Yip
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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
#include "policyprobe.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/click_tcp.h>
#include <click/string.hh>
#include <click/straccum.hh>
#include <click/bitvector.hh>
#include <click/packet_anno.hh>

#define dprintf if(0)printf
#define DELAY .400
#define DONEDELAY 3

PolicyProbe::PolicyProbe(RONRouteModular *parent, 
			 long double delays, 
			 unsigned int numprobes, 
			 int numrandom,
			 long double link_down_penalty,
			 long double link_down_timeout) 
  : RONRouteModular::Policy(parent), _timer(&expire_hook, (void*)this) { 
  assert(numprobes >= numrandom);
  _flowtable = new FlowTable();
  _timerqueue = new TimerQueue(&_timer);
  _history = new RTTHistory();
  _delays = delays;
  _scheduled = 0;
  _numprobes = numprobes;
  _numrandom = numrandom;
  _link_down_penalty = link_down_penalty;
  _link_down_timeout = link_down_timeout;
}
PolicyProbe::~PolicyProbe(){
  delete(_flowtable);
  delete(_timerqueue);
  delete(_history);
}

void PolicyProbe::initialize(int numpaths) {
  _timer.initialize(_parent);
  _numpaths = numpaths;
}

void PolicyProbe::push_forward_syn(Packet *p) {
  int first_syn=0;
  FlowTableEntry *flowentry;
  const click_tcp *tcph= p->tcp_header();
  click_chatter("Forward SYN");

  // Lookup this flow
  flowentry = _flowtable->lookup(IPAddress(p->ip_header()->ip_src),
				 ntohs(tcph->th_sport),
				 IPAddress(p->ip_header()->ip_dst),
				 ntohs(tcph->th_dport));
  // If there's no matching flow, create one
  if (!flowentry) {
    first_syn = 1;
    
    flowentry = _flowtable->insert(IPAddress(p->ip_header()->ip_src), 
				   ntohs(tcph->th_sport),
				   IPAddress(p->ip_header()->ip_dst),
				   ntohs(tcph->th_dport));
  }

  // if it's the first syn, remember <now> for sending on the direct path
  if (first_syn) {
    flowentry->sent_syn(1, gettime());
    // save the pkt for later
    flowentry->syn_pkt = Packet::make(p->data(), p->length());
  }

  // schedule a probe3 timeout for this packet
  _timerqueue->insert(gettime() + _delays, PROBE, flowentry);
  
  // schedule a link down penalty timeout
  _timerqueue->insert(gettime() + _link_down_timeout, NO_SYNACK, flowentry, 1);

  // push along direct path
  _parent->output(1).push(p);
  return;
}
void PolicyProbe::push_forward_fin(Packet *p) {
  FlowTableEntry *flowentry;
  const click_tcp *tcph= p->tcp_header();
  flowentry = _flowtable->lookup(IPAddress(p->ip_header()->ip_src),
				 ntohs(tcph->th_sport),
				 IPAddress(p->ip_header()->ip_dst),
				 ntohs(tcph->th_dport));
  if (!flowentry || !flowentry->chosen_port()) {
    // TODO: set reset?
    p->kill();
    return;
  }
  flowentry->forw_fin();
  if (flowentry->done()){
    _timerqueue->remove(flowentry);
    _timerqueue->insert(gettime() + DONEDELAY, PURGE, flowentry);
  }
  _parent->output(flowentry->chosen_port()).push(p);
  return;
}
void PolicyProbe::push_forward_rst(Packet *p) {
  // TODO:
}
void PolicyProbe::push_forward_normal(Packet *p) {
  FlowTableEntry * flowentry=NULL;
  const click_tcp *tcph= p->tcp_header();

  flowentry = _flowtable->lookup(IPAddress(p->ip_header()->ip_src),
				 ntohs(tcph->th_sport),
				 IPAddress(p->ip_header()->ip_dst),
				 ntohs(tcph->th_dport));
  if (!flowentry || !flowentry->chosen_port()) {
    // TODO: send reset?
    p->kill();
    return;
  }

  _parent->output(flowentry->chosen_port()).push(p);
}
void PolicyProbe::push_reverse_synack(int inport, Packet *p) {
  long double rtt=0;
  FlowTableEntry *flowentry=NULL;
  const click_tcp *tcph= p->tcp_header();
  //click_chatter("Reverse SYN-ACK");

  flowentry = _flowtable->lookup(IPAddress(p->ip_header()->ip_dst),
				 ntohs(tcph->th_dport),
				 IPAddress(p->ip_header()->ip_src), 
				 ntohs(tcph->th_sport));
  if (!flowentry) {
    click_chatter("PoliyProbe: can't find match");
    // TODO: sent rst
    p->kill();
    return;
  }
  
  if (flowentry->get_syn_time(inport) < 0) {
    click_chatter("PoliyProbe: dont remember when syn was sent");
    p->kill();
    return;
  }

  // save this RTT for this port
  //rtt = gettime() - flowentry->get_syn_time(inport);
  flowentry->got_synack(inport);
  _history->add_history(inport, flowentry->get_rtt(inport));
  //_history->punt_old(inport); // do this when reading
  
  if (flowentry->chosen_port()) {
    click_chatter("already chose port");
    //TODO: send reset
    p->kill();
    return;
  }
  
  // remember that we're using this port
  flowentry->choose_port(inport);
  
  // remove this flow from the timeout queue
  _timerqueue->remove(flowentry);

  // forward packet
  _parent->output(0).push(p);
  return;
}
void PolicyProbe::push_reverse_fin(Packet *p) {
  FlowTableEntry *flowentry;
  const click_tcp *tcph= p->tcp_header();
  flowentry = _flowtable->lookup(IPAddress(p->ip_header()->ip_dst),
				 ntohs(tcph->th_dport),
				 IPAddress(p->ip_header()->ip_src),
				 ntohs(tcph->th_sport));
  if (!flowentry || !flowentry->chosen_port()) {
    // TODO: set reset?
    p->kill();
    return;
  }
  flowentry->rev_fin();
  if (flowentry->done()){
    _timerqueue->remove(flowentry);
    _timerqueue->insert(gettime() + DONEDELAY, PURGE, flowentry);
  }
  _parent->output(0).push(p);
  return;
}
void PolicyProbe::push_reverse_rst(Packet *p) {
  // TODO
}
void PolicyProbe::push_reverse_normal(Packet *p) {
  FlowTableEntry * flowentry=NULL;
  const click_tcp *tcph= p->tcp_header();

  flowentry = _flowtable->lookup(IPAddress(p->ip_header()->ip_dst),
			     ntohs(tcph->th_dport),
			     IPAddress(p->ip_header()->ip_src), 
			     ntohs(tcph->th_sport));
  if (!flowentry || !flowentry->chosen_port()) {
    // TODO: send reset?
    p->kill();
    return;
  }

  _parent->output(0).push(p);
}
void PolicyProbe::expire_hook(Timer *, void *thunk) {
  long double time;
  int action, data;
  FlowTableEntry *flowentry;
  PolicyProbe *me = (PolicyProbe*) thunk;

  fprintf(stderr, "Expire Hook time= %Lf\n", gettime());
  me->_timerqueue->print();

  // process each of the queued events
  while((time = me->_timerqueue->get_oldest(&flowentry, &action, &data)) < 
	gettime() && time >= 0){
    me->_timerqueue->shift();

    fprintf(stderr, " dequing %Lf %d %d ", time, action, data);
    flowentry->print();
    fprintf(stderr, "\n");

    switch (action) {
    case PROBE:
      me->send_probes(flowentry, me->_numprobes);
      break;
    case PURGE: 
      me->_flowtable->remove(flowentry);
      break;
    case NO_SYNACK: 
      if (flowentry->get_rtt(data) != 100) {
	//fprintf(stderr, "   Penalizing\n");
	me->_history->add_history(data, me->_link_down_penalty);
      }
      break;
    }
  }
  me->_timerqueue->_scheduled = 0;
  me->_timerqueue->schedule();
}

void PolicyProbe::send_probes(FlowTableEntry *flowentry, int numprobes) {
  int i,j,k,found_already=0, saveme, done, x;
  long double shortest;
  Vector<long double> times;
  Vector<int> best_paths;
  times.resize(_numpaths+1);
  assert(numprobes <= _numpaths);

  fprintf(stderr, " Sending probes(%d of %d)\n", numprobes, _numpaths);
  // pick best <numprobes> which we haven't tried yet.
  for(i=1; i<=_numpaths; i++) { times[i] = _history->get_avg_rtt(i); }
  for(i=1; i<=_numpaths; i++)
    fprintf(stderr, "  5min avg rtts %d %Lf\n", i, times[i]);

  // find best few guesses
  for(i=0; i<=numprobes-_numrandom; i++) {
    saveme = 0;
    shortest = 100;

    for(j=1; j<=_numpaths; j++) {
      found_already = 0;
      //fprintf(stderr, "  %Lf %Lf\n", times[j], shortest);
      if (times[j] < shortest) {
	//fprintf(stderr, "    %d is smallest\n", j);

	for(k=0; k<best_paths.size(); k++)
	  if (best_paths[k] == j) found_already = 1;

	if (!found_already) {
	  //fprintf(stderr, "    %d was not found yet\n", j);
	  shortest = times[j];
	  saveme  = j;
	}
      }
    }
    if (saveme) best_paths.push_back(saveme);
  }

  // pick something random to probe
  x = (numprobes - best_paths.size());
  
  for(i=0; i<x; i++) {
    fprintf(stderr, "here\n");
    do {
      done = 1;
      j = myrandom(_numpaths) + 1;
      for(k=0; k<best_paths.size(); k++) if (best_paths[k] == j) done =0;
    } while(!done);
    fprintf(stderr, "picking random: %d\n", j);
    best_paths.push_back(j);
  }
  
  // send a copy of the syn packet along those paths.
  for(i=0; i<best_paths.size(); i++){
    fprintf(stderr,"  probing paths: %d\n", best_paths[i]);
    _parent->output(best_paths[i]).push(flowentry->syn_pkt->clone());
  }
  
}


PolicyProbe::FlowTableEntry * 
PolicyProbe::FlowTable::insert(IPAddress src, unsigned short sport,
			       IPAddress dst, unsigned short dport) {
  int i;
  for(i=_v.size()-1; i>=0; i--) {
    if (_v[i].match(src,sport,dst,dport)) {
      //_v[i].policy = policy;
      return &_v[i];
    }
  }
  FlowTableEntry e;
  e.initialize(src, sport, dst, dport);
  _v.push_back(e);
  return &_v[_v.size()-1];
}

PolicyProbe::FlowTableEntry * 
PolicyProbe::FlowTable::lookup(IPAddress src, unsigned short sport,
			       IPAddress dst, unsigned short dport) {
  int i;
  for(i=_v.size()-1; i>=0; i--)
    if (_v[i].match(src,sport,dst,dport))
      return &_v[i];

  return NULL;
}

void
PolicyProbe::FlowTable::remove(IPAddress src, unsigned short sport,
			       IPAddress dst, unsigned short dport) {
  int i,j;

  for(i=_v.size()-1; i>=0; i--)
    if (_v[i].match(src,sport,dst,dport)) {
      for(j=i; j<_v.size()-1; j++)
	_v[j] = _v[j+1];
      _v.pop_back();
    }  
}

void PolicyProbe::FlowTable::remove(FlowTableEntry *entry) {
  remove(entry->src, entry->sport, entry->dst, entry->dport);
}

#include <click/vector.cc>
template class Vector<PolicyProbe::FlowTableEntry*>;
template class Vector<PolicyProbe::FlowTableEntry>;
ELEMENT_PROVIDES(PolicyProbe)

