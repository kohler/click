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
#include "ronroutemodular.hh"

class PolicyProbe : public RONRouteModular::Policy {

public:
  PolicyProbe(RONRouteModular *parent, 
	      unsigned int delayms, unsigned int probenum);
  ~PolicyProbe();

  void push_forward_syn(Packet *p) ;
  void push_forward_fin(Packet *p) ;
  void push_forward_rst(Packet *p) ;
  void push_forward_normal(Packet *p) ;

  void push_reverse_synack(int inport, Packet *p) ;
  void push_reverse_fin(Packet *p) ;
  void push_reverse_rst(Packet *p) ;
  void push_reverse_normal(Packet *p) ;

  void expire_hook(Timer *, void *thunk) ;

protected:
  class FlowTable;
  class FlowTableEntry;  
};


/*
  Need a table mapping 
  Flow -> Time syn as sent
       -> Timeout callback pointer
       -> list of ports which I probed so far.
       -> pointer to saved syn packet

*/
class PolicyProbe::FlowTableEntry {
public:
  IPAddress src, dst;
  unsigned short sport, dport; // network order

  FlowTableEntry(IPAddress s, unsigned short sp,
		 IPAddress d, unsigned short dp, int p) {
    src = s; sport = sp;
    dst = d; dport = dp;
  } 
  bool match(IPAddress s, unsigned short sp,
	     IPAddress d, unsigned short dp) {
    return ((src == s) && (dst == d) && (sport == sp) && (dport == dp));
  }
};

class PolicyProbe::FlowTable {
protected:
  Vector<FlowTableEntry> _v;

public:
  FlowTable(){}

  PolicyProbe::FlowTableEntry * 
  insert(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport, int policy);
  
  PolicyProbe::FlowTableEntry * 
  lookup(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport);
  
  void
  remove(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport);
  
};


