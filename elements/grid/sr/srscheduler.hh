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
  static int static_write_debug(const String &arg, Element *e,
				void *, ErrorHandler *errh); 

  static int static_write_packet_timeout(const String &arg, Element *e,
				void *, ErrorHandler *errh); 
  static int static_write_threshold(const String &arg, Element *e,
				void *, ErrorHandler *errh); 
  int set_packet_timeout(ErrorHandler *, unsigned int);
  int set_threshold(ErrorHandler *, int);
  static String static_print_stats(Element *, void *);
  String print_stats();


  static String static_print_debug(Element *, void *);
  static String static_print_packet_timeout(Element *, void *);
  static String static_print_threshold(Element *, void *);
  void run_timer();
private:
  class ScheduleInfo {
  public:
    SRScheduler *_sr;
    Path _p;
    bool _token; // do I have the token?
    int _tokens_passed;
    IPAddress _towards; // which direction the token is going to
    uint32_t _seq;
    bool _active;

    int _packets_rx;
    int _packets_tx;

    struct timeval _first_rx;
    struct timeval _last_tx;
    struct timeval _last_rx;
    struct timeval _last_real;
    ScheduleInfo() : 
      _sr(0),
      _p(), 
      _token(false), 
      _tokens_passed(0),
      _seq(0),
      _active(false),
      _packets_rx(0),
      _packets_tx(0)
    { }

    ScheduleInfo(SRScheduler *sr) : 
      _sr(sr),
      _p(), 
      _token(false), 
      _tokens_passed(0),
      _seq(0),
      _active(false),
      _packets_rx(0),
      _packets_tx(0)
    { }
    bool is_endpoint(IPAddress ip) const{
      if (_p.size() < 1) {
	return false;
      }
      return (_p[0] == ip || _p[_p.size()-1] == ip);
    }

    IPAddress other_endpoint(IPAddress ip) {
      return (ip == _p[0]) ? _p[_p.size()-1] : _p[0];
    }

    bool hop_timedout() const {
      struct timeval hop_expire;
      struct timeval now;
      click_gettimeofday(&now);
      if (!_sr) {
	return false;
      }
      timeradd(&_last_rx, &_sr->_hop_duration, &hop_expire);
      return (timercmp(&_last_rx, &_last_tx, >) && 
	      timercmp(&hop_expire, &now, <));
    }
    bool rt_timedout() const {
      struct timeval expire;
      struct timeval now;
      click_gettimeofday(&now);
      if (!_sr) {
	return false;
      }
      /* round trip timeout is
       * packets expected * max_tx_time * route_len 
       */
      unsigned int rt_duration_ms = 
	(_sr->_threshold * _sr->_max_tx_packet_ms * (_p.size()+1));

      struct timeval rt_duration;
      timerclear(&rt_duration);
      rt_duration.tv_sec = rt_duration_ms/1000;
      rt_duration.tv_usec = (rt_duration_ms % 1000) * 1000;

      timeradd(&_last_tx, &rt_duration, &expire);
      return timercmp(&expire, &now, <);
    }


    bool active_timedout() const {
      struct timeval expire;
      struct timeval now;
      click_gettimeofday(&now);
      if (!_sr) {
	return false;
      }
      timeradd(&_last_real, &_sr->_active_duration, &expire);
      return timercmp(&expire, &now, <);
    }

    bool clear_timedout() const {
      struct timeval expire;
      struct timeval now;
      click_gettimeofday(&now);
      if (!_sr || _active) {
	return false;
      }
      timeradd(&_last_real, &_sr->_clear_duration, &expire);
      return timercmp(&expire, &now, <);
    }
  };
  friend class ScheduleInfo;

  typedef HashMap<Path, ScheduleInfo> ScheduleTable;
  typedef ScheduleTable::const_iterator STIter;
  ScheduleTable _schedules;

  struct timeval _hop_duration;     // 1-packet timeout
  struct timeval _active_duration;  // how long to fake packets before inactive
  struct timeval _clear_duration;   // how long to keep nfo's around after inactive

  unsigned int _max_tx_packet_ms;
  class SRForwarder *_sr_forwarder;

  Vector<NotifierQueue *> _queues;
  NotifierQueue *_queue1;

  

  struct sr_filter {
    SRScheduler *s;
    Path _p;
    sr_filter(SRScheduler *t, Path p) {
      s = t;
      _p = p;
    }
    bool operator()(const Packet *p) {
      return (s) ? s->ready_for(p, _p) : false;
    }
  };
  friend class sr_filter;

  bool _debug_token;

  int _threshold;
  class ScheduleInfo * find_nfo(Path p);
  class ScheduleInfo * create_nfo(Path p);
  void call_switch(int);
#if 0
  void start_hook();
  void end_hook();
#endif
  void start_path(Path p);
  void end_path(Path p);
  bool ready_for(const Packet *, Path p);
#if 0
  static void static_start_hook(Timer *, void *e) { 
    ((SRScheduler *) e)->start_hook(); 
  }
  static void static_end_hook(Timer *, void *e) { 
    ((SRScheduler *) e)->end_hook(); 
  }
#endif

  Timer _timer;
};


CLICK_ENDDECLS
#endif
