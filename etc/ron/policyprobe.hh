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
#include <sys/time.h>
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
  class TimerQueue;
  
  FlowTable *_flowtable;
  TimerQueue *_timerqueue;

  static long double tolongdouble(struct timeval *tv) {
    return tv->tv_sec + (long double)((long double) tv->tv_usec 
				      / (long double)1000000);
  }

};

class PolicyProbe::TimerQueue {
protected:
  Vector<long double> _times;
  Vector<int> _purge;  // if purge[i] == 0, send probe, else remove.
  Vector<FlowTableEntry*> _entries;
public:
  void insert(long double time, int purge, FlowTableEntry *entry) {
    _times.push_back(time);
    _purge.push_back(purge);
    _entries.push_back(entry);
  }
  // returns -1 if there is nothing left in the TimerQueue
  long double get_oldest(FlowTableEntry **entry, int *purge) {
    long double r = -1;
    if (_entries.size() > 0) {
      *entry = _entries[0];
      *purge = _purge[0];
      r      = _times[0];
    }
    return r;
  }
  void shift() {
    if (_entries.size() > 0) {
      _times.pop_back();
      _purge.pop_back();
      _entries.pop_back();
    }
  }
  void remove(FlowTableEntry *entry) {
    int i,j;
    for(i=0; i<_entries.size(); i++) {
      if (entry == _entries[i]) {
	for(j=i; j<_entries.size()-1; j++) {
	  _entries[j] = _entries[j+1];
	  _purge[j] = _purge[j+1];
	  _times[j] = _times[j+1];
	  _entries.pop_back();
	  _purge.pop_back();
	  _times.pop_back();
	}
      }
    }
  }
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
  Vector<int> ports_tried;
  Vector<long double> sent_time;
  Packet *syn_pkt;

  FlowTableEntry() {
    syn_pkt = NULL;
  };
  ~FlowTableEntry() {
    if (syn_pkt) syn_pkt->kill();
  }

  void initialize(IPAddress s, unsigned short sp,
		  IPAddress d, unsigned short dp) {
    src = s; sport = sp;
    dst = d; dport = dp;
  } 

  bool match(IPAddress s, unsigned short sp,
	     IPAddress d, unsigned short dp) {
    return ((src == s) && (dst == d) && (sport == sp) && (dport == dp));
  }
  void sent_syn(int port, long double time) {
    int i;
    for(i=0; i<ports_tried.size(); i++) {
      if (ports_tried[i] == port){
	sent_time[i] = time;
	return;
      }
    }
    ports_tried.push_back(port);
    sent_time.push_back(time);
    assert(ports_tried.size() == sent_time.size());
  }
};

class PolicyProbe::FlowTable {
protected:
  Vector<FlowTableEntry> _v;

public:
  FlowTable(){}

  PolicyProbe::FlowTableEntry * 
  insert(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport);
  
  PolicyProbe::FlowTableEntry * 
  lookup(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport);
  
  void
  remove(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport);
  
};

  
