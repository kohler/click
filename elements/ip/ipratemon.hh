#ifndef IPRATEMON_HH
#define IPRATEMON_HH

/*
 * =c IPRateMonitor(PB, OFF, RATIO, THRESH, MEMORY)
 *
 * =d
 * Monitors network traffic rates. Can monitor either packet or byte rate (per
 * second) to and from an address. When forward or reverse rate for a particular
 * address exceeds THRESH, rates will then be kept for host or subnet addresses
 * within that address. May update each packet's dst_rate_anno and src_rate_anno
 * with the rates for dst or src IP address.
 *
 * Packets coming in one input 0 are inspected on src address. Packets coming in
 * on input 1 are insepected on dst address. This enables IPRateMonitor to
 * annotate packets with both the forward rate and reverse rate.
 *
 * PB: PACKETS or BYTES. Count number of packets or bytes.
 *
 * OFF: offset in packet where IP header starts
 *
 * RATIO: chance that EWMA gets updated before packet is annotated with EWMA
 * value.
 *
 * THRESH: IPRateMonitor further splits a subnet if rate is over THRESH number
 * packets or bytes per second. Always specify value as if RATIO were 1.
 *
 * MEMORY: How much memory can IPRateMonitor use in kilobytes? Minimum of 100 is
 * enforced. 0 is unlimited memory.
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
 * =e Example: 
 * = IPRateMonitor(PACKETS, 0, 0.5, 256, 600);
 *
 * Monitors packet rates. The memory usage is limited to 600K. When rate for a
 * network address (e.g. 18.26.*.*) exceeds 256 packets per second, start
 * monitor subnet or host addresses (e.g. 18.26.4.*).
 *
 * =a IPFlexMonitor
 * =a CompareBlock
 */


#include "glue.hh"
#include "click_ip.h"
#include "element.hh"
#include "ewma.hh"
#include "vector.hh"

#define BYTES 4                         // in one IP address
#define MAX_SHIFT ((BYTES-1)*8)
#define MAX_COUNTERS 256

#define PERIODIC_FOLD_INIT     8192     // every n packets, a fold() is done

struct HalfSecondsTimer {
  static unsigned now()			{ return click_jiffies() >> 3; }
  static unsigned freq()                { return CLICK_HZ >> 3; }
};

class IPRateMonitor : public Element {
public:
  IPRateMonitor();
  ~IPRateMonitor();

  const char *class_name() const		{ return "IPRateMonitor"; }
  const char *default_processing() const	{ return AGNOSTIC; }

  IPRateMonitor *clone() const;
  void notify_ninputs(int);  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void set_resettime()                          { _resettime = MyEWMA::now(); }

  // HACK! Functions for interaction between fold() and ~Stats()
  struct Stats;
  void set_prev(Stats *s)                       { _prev_deleted = s; }
  void set_next(Stats *s)                       { _next_deleted = s; }
  void set_first(Stats *s)                      { _first = s; }
  void set_last(Stats *s)                       { _last = s; }
  // void update_alloced_mem(int m)                { _alloced_mem += m; click_chatter("mem: %d", _alloced_mem); }
  void update_alloced_mem(int m)                { _alloced_mem += m; }

  void push(int port, Packet *p);
  Packet *pull(int port);


private:

  
  /*
  class EmptyEWMA {
  public:
    short shitty_crap_without_any_fucking_content[3];
    int average() const			  { return 500; }
    static unsigned now()		  { return click_jiffies() >> 3; }
    static unsigned freq()		  { return CLICK_HZ >> 3; }
    
    void initialize()                     {}
    void initialize(unsigned now)         {}
    
    inline void update_time(unsigned now) {}
    inline void update_now(int delta)	  {}
    inline void update(unsigned now, int delta) {}
    
    inline void update_time()             {}
    inline void update(int delta)         {}
  };
  */
  

  typedef RateEWMAX<5, 10, HalfSecondsTimer> MyEWMA;
  // typedef EmptyEWMA MyEWMA;
  
  //
  // Counter
  //
  // XXX: two EWMAs can be compressed two a dual-EWMA. They should share time of
  // last update which is now seperate. Takes more space than necessary.
  //
  // one Counter for each address in a subnet
  struct Counter {
    MyEWMA rev_rate;
    MyEWMA fwd_rate;
    Stats *next_level;
  };

  //
  // Stats
  //
  // one Stats for each subnet
  struct Stats {
    Counter *_parent;               // equals NULL for _base->_parent
    Stats *_prev, *_next;           // to maintain age-list

    Counter* counter[MAX_COUNTERS];
    Stats(IPRateMonitor *m);
    ~Stats();

  private:
    IPRateMonitor *_rm;             // XXX: this sucks
  };


#define COUNT_PACKETS 0
#define COUNT_BYTES 1
  unsigned char _pb;                // packets or bytes
  int _offset;                      // offset in packet
  int _thresh;                      // threshold, when to split


#define MEMMAX_MIN      100         // kbytes
  unsigned int _memmax;             // max. memory usage
  unsigned int _ratio;              // inspect 1 in how many packets?

  struct Stats *_base;              // first level stats
  long unsigned int _resettime;     // time of last reset
  unsigned int _alloced_mem;        // total allocated memory
  struct Stats *_first, *_last;     // first and last element in age list


  // HACK! For interaction between fold() and ~Stats()
  Stats *_prev_deleted, *_next_deleted;

  void update_rates(Packet *, bool, bool);
  void update(IPAddress, int, Packet *, bool, bool);
  void forced_fold();
  void fold(int);
  Counter *make_counter(Stats *, unsigned char, MyEWMA *, MyEWMA *);

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
};


//
// Dives in tables based on addr and raises all rates by val.
//
inline void
IPRateMonitor::update(IPAddress saddr, int val, Packet *p,
    bool forward, bool update_ewma)
{
  unsigned int addr = saddr.addr();
  struct Stats *s = _base;
  Counter *c = 0;
  int bitshift;
  unsigned now = MyEWMA::now();
  static unsigned prev_fold_time = now;

  // zoom in to deepest opened level
  for (bitshift = 0; bitshift <= MAX_SHIFT; bitshift += 8) {
    unsigned char byte = (addr >> bitshift) & 0x000000ff;

    // allocate Counter if it doesn't exist yet
    if (!(c = s->counter[byte]))
      if (!(c = make_counter(s, byte, NULL, NULL)))
        return;

    // update is done on every level. Result: Counter has sum of all the rates
    // of its children
    if(update_ewma) {
      if (forward)
        c->fwd_rate.update(now, val);
      else
        c->rev_rate.update(now, val);
    }

    // zoom in on subnet or host
    if (!c->next_level)
      break;
    s = c->next_level;
  }

  // annotate packet with fwd and rev rates for inspection by CompareBlock
  int fwd_rate = c->fwd_rate.average(); 
  p->set_fwd_rate_anno(fwd_rate);
  int rev_rate = c->rev_rate.average(); 
  p->set_rev_rate_anno(rev_rate);

  //
  // Zoom in if a rate exceeds _thresh, but only if
  // 1. this is not the last byte of the IP address (look at bitshift)
  //    (in other words: zooming in more is not possible)
  // 2. allocating a child will not cause a memory limit violation.
  //
  // next_byte is the record-index in the next level Stats. That Counter record
  // in Stats will get the EWMA value of its parent. XXX: should we divide EWMA
  // value by the number of siblings to give it more realistic value or will it
  // drop fast enough by itself if it has a lot of siblings?
  //
  if ((fwd_rate >= _thresh || rev_rate >= _thresh) &&
     ((bitshift < MAX_SHIFT) &&
     (!_memmax || (_alloced_mem + sizeof(Counter) + sizeof(Stats)) <= _memmax)))
  {
    unsigned char next_byte = (addr >> (bitshift+8)) & 0x000000ff;
    if (!(c->next_level = new Stats(this)) ||
       !make_counter(c->next_level, next_byte, &c->fwd_rate, &c->rev_rate))
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

  // Force a fold roughly once per CLICK_HZ jiffies (i.e., once per second)
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
  click_ip *ip = (click_ip *) (p->data() + _offset);
  int val = (_pb == COUNT_PACKETS) ? 1 : ip->ip_len;

  if (forward)
    update(IPAddress(ip->ip_src), val, p, true, update_ewma);
  else
    update(IPAddress(ip->ip_dst), val, p, false, update_ewma);
}

#endif /* IPRATEMON_HH */
