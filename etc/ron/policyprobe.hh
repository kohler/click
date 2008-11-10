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
//#define HISTORY 15

#define QUEUE_BATCH_TIMESPAN 50

class PolicyProbe : public RONRouteModular::Policy {

public:
  static const int PROBE     = 0;
  static const int PURGE     = 1;
  static const int NO_SYNACK = 2;

  PolicyProbe(RONRouteModular *parent,
	      long double delays, unsigned int numprobes, unsigned int numrandom,
	      long double link_down_penalty, long double link_down_timeout,
	      long double history_timeout, int recycle);
  ~PolicyProbe();
  void initialize(int numpaths);
  void push_forward_syn(Packet *p) ;
  void push_forward_fin(Packet *p) ;
  void push_forward_rst(Packet *p) ;
  void push_forward_normal(Packet *p) ;

  void push_reverse_synack(int inport, Packet *p) ;
  void push_reverse_fin(Packet *p) ;
  void push_reverse_rst(Packet *p) ;
  void push_reverse_normal(Packet *p) ;

  static void expire_hook(Timer *, void *thunk) ;

  static int myrandom(int x) {
    // return a number in [0,x)
    return (int) (x * ( (float)(click_random() & 0xfffe) / (float)(0xffff)));
  }

  static long double gettime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + (long double)((long double) tv.tv_usec
				      / (long double)1000000);
  }

protected:
  class FlowTable;
  class FlowTableEntry;
  class TimerQueue;
  class RTTHistory;

  FlowTable *_flowtable;
  TimerQueue *_timerqueue;
  RTTHistory *_history;
  long double _delays, _link_down_penalty, _link_down_timeout, _history_timeout;
  int _numprobes, _numrandom;

  Timer _timer;
  int _scheduled;
  int _recycle;

  void send_probes(FlowTableEntry *flowentry, int numprobes);

  static long double tolongdouble(struct timeval *tv) {
    return tv->tv_sec + (long double)((long double) tv->tv_usec
				      / (long double)1000000);
  }
};

class PolicyProbe::RTTHistory {
protected:
  struct entry {
    long double timestamp;
    long double rtt;
  };
  Vector<entry> _history[15];
  long double _timeout;
public:
  void set_timeout(long double timeout) {_timeout = timeout;}
  void punt_old(int port) {
    int debug = 0;
    int i=0, j=0;
    long double now = gettime();

    if (debug){
      click_chatter("Punting");
      for(i=0; i<_history[port].size(); i++) {
	fprintf(stderr, "before: %.4Lf %.4Lf\n",
		_history[port][i].timestamp, _history[port][i].rtt );
      }
    }

    for (i=0; i<_history[port].size() && _history[port][i].timestamp + _timeout < now; i++);
    for (j=0; j<_history[port].size()-i; j++)
      _history[port][j] = _history[port][j+i];
    for(j=0; j<i; j++)
      _history[port].pop_back();

    if (debug) {
      for(i=0; i<_history[port].size(); i++) {
	fprintf(stderr, "after : %.4Lf %.4Lf\n",
		_history[port][i].timestamp, _history[port][i].rtt );
      }
    }
  }
  void add_history(int port, long double rtt) {
    struct entry e;
    e.timestamp = gettime();
    e.rtt = rtt;
    punt_old(port);
    _history[port].push_back(e);
  }
  long double get_avg_rtt(int port) {
    int i;
    long double sum=0;
    punt_old(port);
    if (!_history[port].size())  return 0;

    for(i=0; i<_history[port].size(); i++)
      sum += _history[port][i].rtt;
    return sum / _history[port].size();
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
  Vector<int> times_tried;
  Vector<long double> sent_time;
  Vector<long double> rtt;
  Packet *syn_pkt;
  unsigned long syn_seq;
  FlowTableEntry *next;


  FlowTableEntry() {
    syn_pkt = NULL;
    _port = 0;
    _forward_up = _reverse_up = 1;
  };
  ~FlowTableEntry() {
    if (syn_pkt) syn_pkt->kill(); // HERE IS THE PROBLEM
  }

  void print() {
    fprintf(stderr, "%s.%d > %s.%d", src.unparse().c_str(), sport, dst.unparse().c_str(), dport);
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
	times_tried[i] = times_tried[i] + 1;
	return;
      }
    }
    ports_tried.push_back(port);
    times_tried.push_back(1);
    sent_time.push_back(time);
    rtt.push_back(100);
    assert(ports_tried.size() == sent_time.size());
  }
  void got_synack(int port) {
    int i;
    for(i=0; i<ports_tried.size(); i++) {
      if (ports_tried[i] == port){
	rtt[i] = gettime() - sent_time[i];
	return;
      }
    }
  }
  long double get_syn_time(int port) {
    int i;
    for(i=0; i<ports_tried.size(); i++) {
      if (ports_tried[i] == port){
	return sent_time[i];
      }
    }
    return -1;
  }
  int get_times_tried(int port) {
    int i;
    for(i=0; i<ports_tried.size(); i++) {
      if (ports_tried[i] == port){
	return times_tried[i];
      }
    }
    return 0;

  }
  long double get_rtt(int port) {
    int i;
    for(i=0; i<ports_tried.size(); i++) {
      if (ports_tried[i] == port){
	return rtt[i];
      }
    }
    return -1;
  }
  void choose_port(int port) {
    assert(!_port);
    if (syn_pkt) {
      syn_pkt->kill();
      syn_pkt = NULL;
    }
    _port = port;
  }
  int  chosen_port() { return _port; }
  void forw_fin() {_forward_up = 0; }
  void rev_fin() {_reverse_up = 0; }
  int done() {return (!_forward_up && !_reverse_up); }

protected:
  int _port;
  int _forward_up, _reverse_up;
};

class PolicyProbe::FlowTable {
protected:
  FlowTableEntry *_head;

public:
  FlowTable(){ _head = NULL;}

  PolicyProbe::FlowTableEntry *
  insert(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport, unsigned long syn_seq);

  PolicyProbe::FlowTableEntry *
  lookup(IPAddress src, unsigned short sport,
	 IPAddress dst, unsigned short dport);

  void remove(IPAddress src, unsigned short sport,
	      IPAddress dst, unsigned short dport);
  void remove(FlowTableEntry *entry);
};

class PolicyProbe::TimerQueue {
  //  older entries are at index 0.
  //  newest entry is at index n-1

protected:
  struct TimerEntry {
    long double time;
    int action, data;
    FlowTableEntry *entry;
    struct TimerEntry *next;
  };
  Timer *_timer;
  TimerEntry *_head;
public:
  int _scheduled;

  TimerQueue(Timer *timer){_timer = timer; _scheduled = 0; _head = NULL;}
  ~TimerQueue() {
    TimerEntry *p = _head, *t;
    while(p) {
      t = p;
      p = p->next;
      free(t);
    }
  }
  void schedule() {
    uint32_t dmsec;
    long double diff;
    assert(_head || !_scheduled);

    if (!_head) {
      if (_scheduled)  _timer->unschedule();
      _scheduled = 0;
      return;
    }

    if (_scheduled) _timer->unschedule();
    diff =  _head->time - gettime();
    if (diff < 0) diff = 0;
    dmsec = (uint32_t) (1000*( diff ));

    //fprintf(stderr, "Scheduling after %ums (%Lf, %Lf)\n", dmsec, _head->time, gettime());
    _timer->schedule_after_ms( dmsec );
    _scheduled = 1;

  }
  void print() {
    struct TimerEntry *p = _head;
    return;
    while(p){
      fprintf(stderr, " timerqueue: %Lf %d %d ", p->time, p->action, p->data);
      p->entry->print();
      fprintf(stderr, "\n");
      p = p->next;
    }
    fprintf(stderr, "\n");
  }
  void insert(long double time, int action, FlowTableEntry *entry, int data=0) {
    struct TimerEntry **last = &_head;
    struct TimerEntry *p = _head;
    struct TimerEntry *t;
    while(p && p->time <= time) {
      last = &(p->next);
      p = p->next;
    }

    t = (struct TimerEntry*) malloc(sizeof(struct TimerEntry));
    t->time = time;
    t->action = action;
    t->entry = entry;
    t->data = data;
    t->next = p;
    *last = t;

    print();
    schedule();
  }
  // returns -1 if there is nothing left in the TimerQueue
  long double get_oldest(FlowTableEntry **entry, int *action, int *data = NULL) {
    long double r = -1;
    if (_head) {
      *entry  = _head->entry;
      *action = _head->action;
      r       = _head->time;
      if (data) *data   = _head->data;
    } else {
      *entry = NULL;
      *action = 0;
      r = -1;
    }
    return r;
  }
  void shift() {
    struct TimerEntry *p;

    if (_head) {
      p = _head->next;
      free(_head);
      _head = p;
    }
  }
  void remove(FlowTableEntry *entry, int data=0) {
    //fprintf(stderr, "timerqueue removing: ");
    //entry->print();
    //fprintf(stderr, "\n");
    struct TimerEntry **last = &_head;
    struct TimerEntry *p = _head;

    while(p) {
      if ( (p->entry == entry) && ((data == -1) || (p->data == data))) {
	*last = p->next;
	free(p);
	p = *last;
      } else {
	last = &(p->next);
	p = p->next;
      }
    }
  }
};



