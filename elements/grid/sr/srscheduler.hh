#ifndef CLICK_SRSCHEDULER_HH
#define CLICK_SRSCHEDULER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/bighashmap.hh>
#include <click/dequeue.hh>
#include "path.hh"
#include <elements/standard/notifierqueue.hh>
CLICK_DECLS

/*
 * =c
 * SRScheduler(IP, ETH, ETHERTYPE, SRCR element, LinkTable element, ARPtable element, 
 *    [METRIC GridGenericMetric], [WARMUP period in seconds])
 * =d
 * DSR-inspired end-to-end ad-hoc routing protocol.
 * Input 0: ethernet packets 
 * Input 1: ethernet data packets from device 
 * Input 2: IP packets from higher layer, w/ ip addr anno.
 * Input 3: IP packets from higher layer for gw, w/ ip addr anno.
 * Output 0: ethernet packets to device (protocol)
 * Output 1: ethernet packets to device (data)
 *
 */


class SRScheduler : public Element {
 public:
  
  SRScheduler();
  ~SRScheduler();
  
  const char *class_name() const		{ return "SRScheduler"; }
  const char *processing() const		{ return "hl/hl"; }
  int initialize(ErrorHandler *);
  SRScheduler *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void push(int, Packet *);
  Packet *pull(int);
  /* handler stuff */
  void add_handlers();
  static int static_clear(const String &arg, Element *e,
			  void *, ErrorHandler *errh); 
  void clear();


private:
  class ScheduleInfo {
  public:
    Path _p;
    bool _token; // do I have the token?
    int _packets_sent;
    struct timeval _last_tx;
    struct timeval _last_rx;
    struct timeval _last_real;
    ScheduleInfo() : 
      _p(), 
      _token(false), 
      _packets_sent(0)
    { }

  };
  
  typedef BigHashMap<Path, ScheduleInfo> ScheduleTable;
  typedef ScheduleTable::const_iterator STIter;
  ScheduleTable _schedules;

  struct timeval _hop_duration;
  struct timeval _rt_duration;
  struct timeval _endpoint_duration;
  struct timeval _real_duration;

  class SRForwarder *_sr_forwarder;

  Vector<NotifierQueue *> _queues;
  NotifierQueue *_queue1;


  bool _debug_token;

  int _threshold;
  class ScheduleInfo * find_nfo(Path p);
  class ScheduleInfo * create_nfo(Path p);
  void call_switch(int);
  void start_hook();
  void end_hook();
  void start_path(Path p);
  void end_path(Path p);
  static void static_start_hook(Timer *, void *e) { 
    ((SRScheduler *) e)->start_hook(); 
  }
  static void static_end_hook(Timer *, void *e) { 
    ((SRScheduler *) e)->end_hook(); 
  }

  void srscheduler_assert_(const char *, int, const char *) const;

};


CLICK_ENDDECLS
#endif
