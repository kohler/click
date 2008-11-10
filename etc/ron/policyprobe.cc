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
#include <clicknet/tcp.h>
#include <click/string.hh>
#include <click/straccum.hh>
#include <click/bitvector.hh>
#include <click/packet_anno.hh>

#define dprintf if(0)printf
#define DONEDELAY 90
#define MAXROUNDS 5

PolicyProbe::PolicyProbe(RONRouteModular *parent,
			 long double delays,
			 unsigned int numprobes,
			 unsigned int numrandom,
			 long double link_down_penalty,
			 long double link_down_timeout,
			 long double history_timeout,
			 int recycle)
  : RONRouteModular::Policy(parent), _timer(&expire_hook, (void*)this) {
  assert(numprobes >= numrandom);
  assert(link_down_timeout < DONEDELAY);

  _flowtable = new FlowTable();
  _timerqueue = new TimerQueue(&_timer);
  _history = new RTTHistory();
  _delays = delays;
  _scheduled = 0;
  _recycle = recycle;
  _numprobes = numprobes;
  _numrandom = numrandom;
  _link_down_penalty = link_down_penalty;
  _link_down_timeout = link_down_timeout;
  _history_timeout = history_timeout;
  _history->set_timeout(history_timeout);
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
  const click_ip  *iph = p->ip_header();
  const click_tcp *tcph= p->tcp_header();
  unsigned long tcp_seq = ntohl(tcph->th_seq) +
    ntohs(iph->ip_len) - (iph->ip_hl << 2) - (tcph->th_off << 2)+1;

  //click_chatter("Forward SYN: %u", tcp_seq);

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
				   ntohs(tcph->th_dport), tcp_seq);
  }

  if (!flowentry->syn_pkt)
    // save the pkt for later
    //flowentry->syn_pkt = Packet::make(p->data(), p->length());
    flowentry->syn_pkt = p->clone();


  // if it's the first syn, remember <now> for sending on the direct path
  if (flowentry->get_syn_time(1) < 0) {
    flowentry->sent_syn(1, gettime());
    // schedule a link down penalty timeout
    _timerqueue->insert(gettime() + _link_down_timeout,
			NO_SYNACK, flowentry, 1);

  }

  // schedule a probe3 timeout for this packet
  _timerqueue->insert(gettime() + _delays, PROBE, flowentry);

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
    //_timerqueue->remove(flowentry, 0);
    _timerqueue->insert(gettime() + DONEDELAY, PURGE, flowentry);
  }
  _parent->output(flowentry->chosen_port()).push(p);
  return;
}
void PolicyProbe::push_forward_rst(Packet *p) {
  FlowTableEntry *flowentry;
  const click_tcp *tcph= p->tcp_header();
  flowentry = _flowtable->lookup(IPAddress(p->ip_header()->ip_src),
				 ntohs(tcph->th_sport),
				 IPAddress(p->ip_header()->ip_dst),
				 ntohs(tcph->th_dport));
  if(!flowentry || !flowentry->chosen_port()) {
    p->kill();
    return;
  }
  _parent->output(flowentry->chosen_port()).push(p);
  return;
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
  FlowTableEntry *flowentry=NULL;
  const click_tcp *tcph= p->tcp_header();
  //click_chatter("Reverse SYN-ACK on port %d", inport);

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
    //click_chatter("  already chose port");
    if (flowentry->chosen_port() == inport) {
       _parent->output(0).push(p);
       return;
    }

    //fprintf(stderr, "  port %d received synack in %Lf\n", inport, flowentry->get_rtt(inport));
    fprintf(stderr, "RTT___%02d: %Lf: ", (ntohs(tcph->th_dport) / 100 % 20), gettime());
    flowentry->print();
    fprintf(stderr, ": port %02d rtt %Lf\n", inport, flowentry->get_rtt(inport));

    _timerqueue->remove(flowentry, inport);
    _parent->send_rst(p, flowentry->syn_seq, inport);
    //p->kill(); // send_rst already does this
    return;
  }

  // remember that we're using this port
  flowentry->choose_port(inport);
  fprintf(stderr, "CHOSE_%02d: %Lf: ",   (ntohs(tcph->th_dport) / 100 % 20), gettime());
  flowentry->print();
  fprintf(stderr, ": port %02d\n", inport);

  // remove this flow from the timeout queue
  _timerqueue->remove(flowentry, 0);
  _timerqueue->remove(flowentry, inport);

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
    //_timerqueue->remove(flowentry, 0);
    _timerqueue->insert(gettime() + DONEDELAY, PURGE, flowentry);
  }
  _parent->output(0).push(p);
  return;
}
void PolicyProbe::push_reverse_rst(Packet *p) {
  FlowTableEntry *flowentry;
  const click_tcp *tcph= p->tcp_header();
  flowentry = _flowtable->lookup(IPAddress(p->ip_header()->ip_dst),
				 ntohs(tcph->th_dport),
				 IPAddress(p->ip_header()->ip_src),
				 ntohs(tcph->th_sport));
  if(!flowentry || !flowentry->chosen_port()) {
    p->kill();
    return;
  }
  _parent->output(0).push(p);
  return;
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

  //fprintf(stderr, "Expire Hook time= %Lf\n", gettime());
  //me->_timerqueue->print();

  // process each of the queued events
  while((time = me->_timerqueue->get_oldest(&flowentry, &action, &data)) <
	(gettime() + (long double)((long double)QUEUE_BATCH_TIMESPAN / (long double)1000)) &&
	time >= 0){
    me->_timerqueue->shift();

    //fprintf(stderr, " dequing %Lf %d %d ", time, action, data);
    //flowentry->print();
    //fprintf(stderr, "\n");

    switch (action) {
    case PROBE:
      me->send_probes(flowentry, me->_numprobes);
      break;
    case PURGE:
      me->_timerqueue->remove(flowentry, -1);
      me->_flowtable->remove(flowentry);
      break;
    case NO_SYNACK:
      //if (flowentry->get_rtt(data) != 100) {
      //fprintf(stderr, "   PENALIZING\n");
      me->_history->add_history(data, me->_link_down_penalty);
      //}
      break;
    }
  }
  me->_timerqueue->_scheduled = 0;
  me->_timerqueue->schedule();
}

void PolicyProbe::send_probes(FlowTableEntry *flowentry, int numprobes) {
  int i,j,k,found_already=0, saveme, done = 0, x;
  int round=0, exhausted_round=0;
  long double shortest;
  Vector<long double> times;
  Vector<int> best_paths;
  const click_tcp *tcph;
  times.resize(_numpaths+1);
  assert(numprobes < _numpaths);
  assert(_numrandom <= _numprobes);

  //fprintf(stderr, " Sending probes(%d of %d)\n", numprobes, _numpaths);
  // pick best <numprobes> which we haven't tried yet.
  for(i=1; i<=_numpaths; i++) { times[i] = _history->get_avg_rtt(i); }
  //for(i=1; i<=_numpaths; i++)
    //fprintf(stderr, "  5min avg rtts %d %.4Lf %d\n",
    //    i, times[i], flowentry->get_times_tried(i));

  // pick the best (numprobes - numrandom) guesses
  if (_recycle) { // RECYCLE

    // figure out what the current round number is:
    round = flowentry->get_times_tried(2);
    for(i=3; i<=_numpaths; i++)
      if (round > flowentry->get_times_tried(i))
	round = flowentry->get_times_tried(i);

    for(i=round; !done && i<MAXROUNDS; i++) {
      exhausted_round = 0;

      while(!exhausted_round) {
	saveme=0;
	shortest = 100;
	for(j=2; !done && j<=_numpaths; j++) {
	  if (flowentry->get_times_tried(j) == i) {
	    if (times[j] <= shortest) {
	      found_already = 0;
	      for(k=0; !found_already && k<best_paths.size(); k++)
		if (best_paths[k] == j) found_already = 1;
	      if (!found_already){
		shortest = times[j];
		saveme = j;
	      }
	    }
	  }
	}
	if (!saveme) exhausted_round = 1;
	else {
	  if (best_paths.size() < (numprobes - _numrandom)) {
	    for(k=0; k<best_paths.size(); k++)
	      assert (best_paths[k] != saveme);
	    best_paths.push_back(saveme);
	  }

	  if (best_paths.size() == (numprobes - _numrandom)) {
	    exhausted_round = 1;
	    done = 1;
	  }
	}
      }
    }

  } else if (flowentry->syn_pkt) { // NO RECYCLE, just pick n best guesses

    // if we already sent probes, just use the same paths
    for(j=2; j<=_numpaths; j++) {
      if (flowentry->get_times_tried(j) > 0) {
	best_paths.push_back(j);
      }
    }

    // if we have not sent probes yet, figure out which probes to make
    if (best_paths.size() == 0) {
      // if this is the first syn, then pick the best paths.
      for(i=0; i<(numprobes - _numrandom); i++) {
	shortest = 100;
	done = 0;

	for(j=2; !done && j<=_numpaths; j++) {

	  if (times[j] <= shortest) {
	    found_already = 0;
	    for(k=0; !found_already && k<best_paths.size(); k++)
	      if (best_paths[k] == j) found_already = 1;
	    if (!found_already){
	      shortest = times[j];
	      saveme = j;
	      done = 1;
	    }
	  }
	}
	assert (saveme);
	if (best_paths.size() < (numprobes - _numrandom)) {
	  for(k=0; k<best_paths.size(); k++)
	    assert (best_paths[k] != saveme);
	  best_paths.push_back(saveme);
	}
      }
    }
  }

  // pick x random paths to probe
  x = (numprobes - best_paths.size());

  for(i=0; i<x; i++) {
    do {
      done = 1;
      j = myrandom(_numpaths-1) + 2;
      for(k=0; k<best_paths.size(); k++) if (best_paths[k] == j) done =0;
    } while(!done);
    //fprintf(stderr, "picking random: %d\n", j);
    best_paths.push_back(j);
  }

  assert (flowentry->syn_pkt);
  tcph= flowentry->syn_pkt->tcp_header();
  fprintf(stderr, "PROBES%02d: %Lf: ", (ntohs(tcph->th_sport) / 100 % 20), gettime());
  flowentry->print();
  fprintf(stderr, ": probing");

  // send a copy of the syn packet along those paths.
  for(i=0; i<best_paths.size(); i++){
    fprintf(stderr," %d", best_paths[i]);
    if (flowentry->get_syn_time(best_paths[i]) < 0) {
      _timerqueue->insert(gettime() + _link_down_timeout,
			  NO_SYNACK, flowentry, best_paths[i]);
    }
    flowentry->sent_syn(best_paths[i], gettime());
    _parent->output(best_paths[i]).push(flowentry->syn_pkt->clone());
  }
  fprintf(stderr,"\n");
}


PolicyProbe::FlowTableEntry *
PolicyProbe::FlowTable::insert(IPAddress src, unsigned short sport,
			       IPAddress dst, unsigned short dport,
			       unsigned long syn_seq) {
  FlowTableEntry* p = _head;
  FlowTableEntry* newentry;

  // If there's a match, return it.
  while(p) {
    if (p->match(src,sport,dst,dport))
      return p;
    p = p->next;
  }

  // otherwise, insert a new one.
  newentry = new FlowTableEntry();
  newentry->initialize(src, sport, dst, dport);
  newentry->next = _head;
  newentry->syn_seq = syn_seq;
  _head = newentry;
  return _head;
}

PolicyProbe::FlowTableEntry *
PolicyProbe::FlowTable::lookup(IPAddress src, unsigned short sport,
			       IPAddress dst, unsigned short dport) {
  FlowTableEntry* p = _head;
  while(p) {
    if (p->match(src,sport,dst,dport))
      return p;
    p = p->next;
  }
  return NULL;
}

void
PolicyProbe::FlowTable::remove(IPAddress src, unsigned short sport,
			       IPAddress dst, unsigned short dport) {
  FlowTableEntry* p = _head;
  FlowTableEntry** last = &_head;

  while(p) {
    if (p->match(src,sport,dst,dport)) {
      *last = p->next;
      delete(p);
      p = *last;
    } else {
      last = &(p->next);
      p = p->next;
    }
  }
}

void PolicyProbe::FlowTable::remove(FlowTableEntry *entry) {
  /*
  fprintf (stderr, "  want to remove: ");
  entry->print();
  fprintf(stderr, "\n");
  */
  remove(entry->src, entry->sport, entry->dst, entry->dport);
}

template class Vector<PolicyProbe::FlowTableEntry*>;
template class Vector<PolicyProbe::FlowTableEntry>;
ELEMENT_PROVIDES(PolicyProbe)

