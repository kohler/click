/*
 * ronroutemodular.{cc,hh} -- element looks up next-hop address for NAT-RON
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
#include "ronroutemodular.hh"
#include "policyprobe.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/tcp.h>
#include <click/string.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>

#define dprintf if(0)click_chatter
#define d2printf if(1)click_chatter

RONRouteModular::RONRouteModular() {
  _flowtable = new FlowTable();

  _policies.push_back(new PolicyProbe(this, .4, 3, 1, 75, 75, 300, 1));
  _policies.push_back(new PolicyProbe(this, .4, 2, 1, 75, 75, 300, 1));
  _policies.push_back(new PolicyProbe(this, .2, 3, 1, 75, 75, 300, 1));
  _policies.push_back(new PolicyProbe(this, .4, 3, 1, 75, 75, 150, 1));
  _policies.push_back(new PolicyProbe(this, .4, 3, 1, 75, 75, 300, 0));
}

RONRouteModular::~RONRouteModular(){
  int i;

  delete(_flowtable);
  for(i=0; i<_policies.size(); i++)
    delete(_policies[i]);
}

int
RONRouteModular::configure(const Vector<String> &conf, ErrorHandler *errh){
  return 0;
}

int
RONRouteModular::initialize(ErrorHandler *){
  int i;
  for(i=0; i<_policies.size(); i++)
    _policies[i]->initialize(noutputs()-1);

  return 0;
}

void RONRouteModular::push(int inport, Packet *p)
{

  if (inport == 0) {
    push_forward_packet(p);
  } else {
    push_reverse_packet(inport, p);
  }
}

void RONRouteModular::push_forward_packet(Packet *p)
{
  const click_tcp *tcph;
  int policy = PAINT_ANNO(p);

  //click_chatter("SAW FORWARD PKT color: %d",  PAINT_ANNO(p) );

  // Verify policy is in valid range.
  //d2printf("policies size: %d", _policies.size());
  if (policy >= _policies.size()){
    d2printf(" No such policy: %d", policy);
    p->kill();
    return;
  }

  // if non-TCP just forward direct
  if (p->ip_header()->ip_p != IP_PROTO_TCP) {
    dprintf("non-TCP proto(%d)", p->ip_header()->ip_p);
    output(1).push(p);
    return;
  }

  // Switch on TCP packet type
  // TODO: save seq number
  tcph = p->tcp_header();
  _flowtable->insert(IPAddress(p->ip_header()->ip_src), ntohs(tcph->th_sport),
		     IPAddress(p->ip_header()->ip_dst), ntohs(tcph->th_dport),
		     policy);

  if (tcph->th_flags & TH_SYN) {
    _policies[policy]->push_forward_syn(p);
  } else if (tcph->th_flags & TH_FIN) {
    _policies[policy]->push_forward_fin(p);
  } else if (tcph->th_flags & TH_RST) {
    _policies[policy]->push_forward_rst(p);
  } else {
    _policies[policy]->push_forward_normal(p);
  }
  //click_chatter("");
  return;
}

void RONRouteModular::push_reverse_packet(int inport, Packet *p)
{
  const click_tcp *tcph;
  FlowTableEntry *entry;
  //d2printf("SAW REVERSE PACKET");

  // if non-TCP just forward direct
  if (p->ip_header()->ip_p != IP_PROTO_TCP) {
    dprintf("non-TCP proto(%d)", p->ip_header()->ip_p);
    output(0).push(p);
    return;
  }

  tcph = p->tcp_header();
  entry = _flowtable->lookup(IPAddress(p->ip_header()->ip_dst),
			     ntohs(tcph->th_dport),
			     IPAddress(p->ip_header()->ip_src),
			     ntohs(tcph->th_sport));

  if (!entry) {
    d2printf(" Could not find flow");
    // TODO: send reset
    p->kill();
    return;
  }

  // Switch on TCP packet type
  tcph = p->tcp_header();
  if ((tcph->th_flags & TH_SYN) && (tcph->th_flags & TH_ACK)) {
    _policies[entry->policy]->push_reverse_synack(inport, p);
  } else if (tcph->th_flags & TH_FIN) {
    _policies[entry->policy]->push_reverse_fin(p);
  } else if (tcph->th_flags & TH_RST) {
    _policies[entry->policy]->push_reverse_rst(p);
  } else {
    _policies[entry->policy]->push_reverse_normal(p);
  }
  return;
}


void RONRouteModular::send_rst(Packet *p, unsigned long seq, int outport) {
  WritablePacket *rst_pkt;
  click_ip *iphdr;
  click_tcp *tcphdr;

  //click_chatter("SENDING RST: port %d seq: %u\n", outport, seq);

  rst_pkt = WritablePacket::make(40);
  rst_pkt->set_network_header(rst_pkt->data(), 20);
  iphdr  = rst_pkt->ip_header();
  tcphdr = rst_pkt->tcp_header();

  tcphdr->th_sport = p->tcp_header()->th_dport;
  tcphdr->th_dport = p->tcp_header()->th_sport;
  tcphdr->th_seq   = htonl(seq);
  tcphdr->th_ack   = htonl(ntohl(p->tcp_header()->th_seq) + 1);
  tcphdr->th_off   = 5;
  tcphdr->th_flags  = TH_RST | TH_ACK;
  tcphdr->th_win   = ntohs(16384);
  tcphdr->th_urp   = 0;
  tcphdr->th_sum   = 0;

  memset(iphdr, '\0', 9);
  iphdr->ip_sum = 0;
  iphdr->ip_len = htons(20);
  iphdr->ip_p   = IP_PROTO_TCP;
  iphdr->ip_src = p->ip_header()->ip_dst;
  iphdr->ip_dst = p->ip_header()->ip_src;

  //set tcp checksum
  tcphdr->th_sum = click_in_cksum((unsigned char *)iphdr, 40);
  iphdr->ip_len = htons(40);

  iphdr->ip_v   = 4;
  iphdr->ip_hl  = 5;
  iphdr->ip_id  = htons(0x1234);
  iphdr->ip_off = 0;
  iphdr->ip_ttl = 32;
  iphdr->ip_sum = 0;

  // set ip checksum
  iphdr->ip_sum = click_in_cksum(rst_pkt->data(), 20);

  p->kill();
  output(outport).push(rst_pkt);
  return;
}
int
RONRouteModular::myrandom(int x) {
  return (int) (x * ( (float)(click_random() & 0xfffe) / (float)(0xffff)));
}
void RONRouteModular::duplicate_pkt(Packet *p) {
}

void RONRouteModular::expire_hook(Timer *, void *thunk) {
}

static String read_handler(Element *, void *){ return "false\n"; }

void RONRouteModular::add_handlers(){
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}

void RONRouteModular::print_time(char* s) {
  struct timeval tp;
  gettimeofday(&tp, NULL);
  click_chatter("%s (%ld.%06ld)", s, tp.tv_sec & 0xffff, tp.tv_usec);
}

RONRouteModular::FlowTableEntry *
RONRouteModular::FlowTable::insert(IPAddress src, unsigned short sport,
				   IPAddress dst, unsigned short dport,
				   int policy) {
  int i;
  for(i=_v.size()-1; i>=0; i--)
    if (_v[i].match(src,sport,dst,dport)) {
      _v[i].policy = policy;
      return &_v[i];
    }

  FlowTableEntry e(src, sport, dst, dport, policy);
  _v.push_back(e);
  //  d2printf(" inserting %s(%d) -> %s(%d)", src.unparse().c_str(), _v[_v.size()-1].sport, dst.unparse().c_str(), _v[_v.size()-1].dport);
  return &_v[_v.size()-1];
}

RONRouteModular::FlowTableEntry *
RONRouteModular::FlowTable::lookup(IPAddress src, unsigned short sport,
				   IPAddress dst, unsigned short dport) {
  int i;
  for(i=_v.size()-1; i>=0; i--){
    if (_v[i].match(src,sport,dst,dport))
      return &_v[i];
  }

  return NULL;
}

void
RONRouteModular::FlowTable::remove(IPAddress src, unsigned short sport,
				   IPAddress dst, unsigned short dport) {
  int i,j;
  for(i=_v.size()-1; i>=0; i--)
    if (_v[i].match(src,sport,dst,dport)) {
      for(j=i; j<_v.size()-1; j++)
	_v[j] = _v[j+1];
      _v.pop_back();
    }
}


// must always generate the whole template instance! RONRouteModular demands it
template class Vector<RONRouteModular::Policy*>;
//template class Vector<RONRouteModular::FlowTableEntry>;
EXPORT_ELEMENT(RONRouteModular)
