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

#define dprintf if(0)printf

PolicyProbe::PolicyProbe(RONRouteModular *parent, 
			 unsigned int delayms, unsigned int probenum ) 
  : RONRouteModular::Policy(parent)
{ 
  
  _flowtable = new FlowTable();
}
PolicyProbe::~PolicyProbe(){
}

void PolicyProbe::push_forward_syn(Packet *p) {
  int first_syn=0;
  FlowTableEntry *entry;
  struct timeval tv;
  const click_tcp *tcph= p->tcp_header();

  click_chatter("SAW FORWARD PKT");

  // Lookup this flow
  entry = _flowtable->lookup(p->ip_header()->ip_src, ntohs(tcph->th_sport),
			     p->ip_header()->ip_dst, ntohs(tcph->th_dport));
  // If there's no matching flow, create one
  if (!entry) {
    first_syn = 1;
    entry = _flowtable->insert(p->ip_header()->ip_src, ntohs(tcph->th_sport),
			       p->ip_header()->ip_dst, ntohs(tcph->th_dport));
  }

  if (first_syn) {
    gettimeofday(&tv, NULL);
    entry->sent_syn(1, tolongdouble(&tv));
    entry->syn_pkt = p->clone(); // save the pkt for later
  }

}
void PolicyProbe::push_forward_fin(Packet *p) {
}
void PolicyProbe::push_forward_rst(Packet *p) {
}
void PolicyProbe::push_forward_normal(Packet *p) {
}
void PolicyProbe::push_reverse_synack(int inport, Packet *p) {
}
void PolicyProbe::push_reverse_fin(Packet *p) {
}
void PolicyProbe::push_reverse_rst(Packet *p) {
}
void PolicyProbe::push_reverse_normal(Packet *p) {
}
void PolicyProbe::expire_hook(Timer *, void *thunk) {
}




PolicyProbe::FlowTableEntry * 
PolicyProbe::FlowTable::insert(IPAddress src, unsigned short sport,
			       IPAddress dst, unsigned short dport) {
  int i;
  for(i=_v.size()-1; i>=0; i--)
    if (_v[i].match(src,sport,dst,dport)) {
      //_v[i].policy = policy;
      return &_v[i];
    }
  
  FlowTableEntry e(src, sport, dst, dport);
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

#include <click/vector.cc>
template class Vector<PolicyProbe::FlowTableEntry*>;
ELEMENT_PROVIDES(PolicyProbe)

