#ifndef CLICK_IPRATEMON_HH
#define CLICK_IPRATEMON_HH

/*
 * =c
 * IPRateMonitor(PB, RATIO, THRESH [, MEMORY, ANNO])
 * =s IP, measurement
 * measures coming and going IP traffic rates
 *
 * =d
 * Monitors network traffic rates. Can monitor either packet or byte rate (per
 * second) to and from an address. When forward or reverse rate for a
 * particular address exceeds THRESH, rates will then be kept for host or
 * subnet addresses within that address. May update each packet's
 * dst_rate_anno and src_rate_anno with the rates for dst or src IP address.
 *
 * Packets coming in one input 0 are inspected on src address. Packets coming
 * in on input 1 are insepected on dst address. This enables IPRateMonitor to
 * annotate packets with both the forward rate and reverse rate.
 *
 * PB: PACKETS or BYTES. Count number of packets or bytes.
 *
 * RATIO: chance that EWMA gets updated before packet is annotated with EWMA
 * value.
 *
 * THRESH: IPRateMonitor further splits a subnet if rate is over THRESH number
 * packets or bytes per second. Always specify value as if RATIO were 1.
 *
 * MEMORY: How much memory can IPRateMonitor use in kilobytes? Minimum of 100
 * is enforced. 0 is unlimited memory.
 *
 * ANNO: if on (by default, it is), annotate packets with rates.
 *
 * =h look (read)
 * Returns the rate of counted to and from a cluster of IP addresses. The first
 * printed line is the number of 'jiffies' that have past since the last reset.
 * There are 100 jiffies in one second.
 *
 * =h thresh (read)
 * Returns THRESH.
 *
 * =h reset (write)
 * When written, resets all rates.
 *
 * =h anno_level (write)
 * Expects "IPAddress level when". When written, makes IPRateMonitor stop
 * expanding at "level" (0-3) for the IPAddress, until "when" (seconds).  For
 * example, if "18.26.4.0 2 100" is specified, IPRateMonitor will stop
 * expanding when 18.26.4 is reached for the next 100 seconds. After 100
 * seconds, any level below 18.26.4 may be reached again.
 *
 * =e
 *   IPRateMonitor(PACKETS, 0.5, 256, 600);
 *
 * Monitors packet rates. The memory usage is limited to 600K. When rate for a
 * network address (e.g. 18.26.*.*) exceeds 256 packets per second, start
 * monitor subnet or host addresses (e.g. 18.26.4.*).
 *
 * =a IPFlexMonitor, CompareBlock */

#include <click/glue.hh>
#include <click/click_ip.h>
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/vector.hh>
#include <click/packet_anno.hh>

struct IPRateMonitor_HalfSecondsTimer {
  static unsigned now()			{ return click_jiffies() >> 3; }
  static unsigned freq()                { return CLICK_HZ >> 3; }
};

class Spinlock;

class IPRateMonitor : public Element {
public:
  IPRateMonitor();
  ~IPRateMonitor();

  const char *class_name() const		{ return "IPRateMonitor"; }
  const char *default_processing() const	{ return AGNOSTIC; }

  IPRateMonitor *clone() const;
  void notify_ninputs(int);  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);

  void set_resettime()                          { _resettime = MyEWMA::now(); }
  void set_anno_level(unsigned addr, unsigned level, unsigned when);

  void push(int port, Packet *p);
  Packet *pull(int port);

  int llrpc(unsigned, void *);

private:

  typedef RateEWMAX<5, 10, 2, IPRateMonitor_HalfSecondsTimer> MyEWMA;
  
  struct Stats;
  struct Counter {
    // fwd_and_rev_rate.average[0] is forward rate
    // fwd_and_rev_rate.average[1] is reverse rate
    MyEWMA fwd_and_rev_rate;
    Stats *next_level;
    unsigned anno_this;
  };

  // one Stats for each subnet
  static const int MAX_COUNTERS = 256;
  struct Stats {
    Counter *_parent;               // equals NULL for _base->_parent
    Stats *_prev, *_next;           // to maintain age-list

    Counter* counter[MAX_COUNTERS];
    Stats(IPRateMonitor *m);
    ~Stats();

  private:
    IPRateMonitor *_rm;             // XXX: this sucks
  };

protected:
  
  // HACK! Functions for interaction between fold() and ~Stats()
  friend struct Stats;
  void set_prev(Stats *s)                       { _prev_deleted = s; }
  void set_next(Stats *s)                       { _next_deleted = s; }
  void set_first(Stats *s)                      { _first = s; }
  void set_last(Stats *s)                       { _last = s; }
  void update_alloced_mem(int m)                { _alloced_mem += m; }

private:

  static const int MAX_SHIFT = 24;
  // every n packets, a fold() is done
  static const int PERIODIC_FOLD_INIT = 8192; 
  static const unsigned MEMMAX_MIN = 100; // kbytes
  
  bool _count_packets;		// packets or bytes
  bool _anno_packets;		// annotate packets?
  int _thresh;			// threshold, when to split
  unsigned int _memmax;		// max. memory usage
  unsigned int _ratio;		// inspect 1 in how many packets?
  Spinlock* _lock;		// synchronize handlers and update

  struct Stats *_base;          // first level stats
  long unsigned int _resettime;     // time of last reset
  unsigned int _alloced_mem;        // total allocated memory
  struct Stats *_first, *_last;     // first and last element in age list
  // HACK! For interaction between fold() and ~Stats()
  Stats *_prev_deleted, *_next_deleted;

  void update_rates(Packet *, bool, bool);
  void update(unsigned, int, Packet *, bool, bool);
  void forced_fold();
  void fold(int);
  Counter *make_counter(Stats *, unsigned char, MyEWMA *);

  void show_agelist(void);

  String print(Stats *s, String ip = "");

  void add_handlers();
  static String thresh_read_handler(Element *e, void *);
  static String look_read_handler(Element *e, void *);
  static String what_read_handler(Element *e, void *);
  static String mem_read_handler(Element *e, void *);
  static String memmax_read_handler(Element *e, void *);
  static int reset_write_handler
    (const String &, Element *, void *, ErrorHandler *);
  static int memmax_write_handler
    (const String &, Element *, void *, ErrorHandler *);
  static int anno_level_write_handler
    (const String &, Element *, void *, ErrorHandler *);
};

inline void
IPRateMonitor::set_anno_level(unsigned addr, unsigned level, unsigned when)
{
  struct Stats *s = _base;
  Counter *c = 0;
  int bitshift;

  addr = ntohl(addr);

  // zoom in to the specified level
  for (bitshift = 24; bitshift >= 0; bitshift -= 8) {
    unsigned char byte = (addr >> bitshift) & 255;

    if (!(c = s->counter[byte]))
      return;

    if (level == 0) {
      c->anno_this = when;
      delete c->next_level;
      c->next_level = 0;
      return;
    }

    if (!c->next_level)
      return;

    s = c->next_level;
    level--;
  }
}

//
// Dives in tables based on addr and raises all rates by val.
//
inline void
IPRateMonitor::update(unsigned addr, int val, Packet *p, 
                      bool forward, bool update_ewma)
{
  struct Stats *s = _base;
  Counter *c = 0;
  unsigned now = MyEWMA::now();
  static unsigned prev_fold_time = now;

  // zoom in to deepest opened level
  addr = ntohl(addr);		// need it in network order
  
  int bitshift;
  for (bitshift = 24; bitshift >= 0; bitshift -= 8) {
    unsigned char byte = (addr >> bitshift) & 255;

    // allocate Counter if it doesn't exist yet
    if (!(c = s->counter[byte]))
      if (!(c = make_counter(s, byte, NULL)))
        return;

    // update is done on every level. Result: Counter has sum of all the rates
    // of its children
    if(update_ewma) {
      if (forward)
        c->fwd_and_rev_rate.update(val,0);
      else
        c->fwd_and_rev_rate.update(val,1);
    }
    
    // zoom in on subnet or host
    if (!c->next_level)
      break;
    s = c->next_level;
  }
    
  int fwd_rate = c->fwd_and_rev_rate.average(0); 
  int rev_rate = c->fwd_and_rev_rate.average(1); 
  int scale = c->fwd_and_rev_rate.scale;
  int freq = c->fwd_and_rev_rate.freq();
  fwd_rate = (fwd_rate * freq) >> scale;
  rev_rate = (rev_rate * freq) >> scale;

  if (_anno_packets) {
    // annotate packet with fwd and rev rates for inspection by CompareBlock
    SET_FWD_RATE_ANNO(p, fwd_rate);
    SET_REV_RATE_ANNO(p, rev_rate);
  }

  //
  // Zoom in if a rate exceeds _thresh, but only if
  // 1. this is not the last byte of the IP address (look at bitshift)
  //    (in other words: zooming in more is not possible)
  // 2. allocating a child will not cause a memory limit violation.
  //
  // next_byte is the record-index in the next level Stats. That Counter
  // record in Stats will get the EWMA value of its parent.
  //
  if (c->anno_this < now &&
      (fwd_rate >= _thresh || rev_rate >= _thresh) &&
      ((bitshift > 0) &&
      (!_memmax || (_alloced_mem+sizeof(Counter)+sizeof(Stats)) <= _memmax)))
  {
    bitshift -= 8;
    unsigned char next_byte = (addr >> bitshift) & 255;
    if (!(c->next_level = new Stats(this)) ||
       !make_counter(c->next_level, next_byte, &c->fwd_and_rev_rate))
    {
      if(c->next_level) {     // new Stats() may have succeeded: kill it.
        delete c->next_level;
        c->next_level = 0;
      }
      return;
    }
    
    // tell parent about newly created Stats and make it youngest in age-list
    c->next_level->_parent = c;

    // append to end of list
    _last->_next = c->next_level;
    c->next_level->_next = 0;
    c->next_level->_prev = _last;
    _last = c->next_level;
  }

  if(now - prev_fold_time >= MyEWMA::freq()) {
    fold(_thresh);
    prev_fold_time = now;
  }
}


// for forward packets (port 0), update based on src IP address;
// for reverse packets (port 1), update based on dst IP address.
inline void
IPRateMonitor::update_rates(Packet *p, bool forward, bool update_ewma)
{
  const click_ip *ip = p->ip_header();
  int val = _count_packets ? 1 : ntohs(ip->ip_len);

  if (forward)
    update(ip->ip_src.s_addr, val, p, true, update_ewma);
  else
    update(ip->ip_dst.s_addr, val, p, false, update_ewma);
}

#endif /* IPRATEMON_HH */
