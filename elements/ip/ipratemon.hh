#ifndef IPRATEMON_HH
#define IPRATEMON_HH

/*
 * =c IPRateMonitor(PB, OFF, RATIO, THRESH, MEMORY)
 *
 * =d
 * Monitors network traffic rates. Can monitor either packet or byte rate (per
 * second) to and from an address. When the to or from rate for a particular
 * address exceeds the threshold, rates will then be kept for host or subnet
 * addresses within that address. May update each packet's dst_rate_anno and
 * src_rate_anno with the rates for dst or src IP address (specified by the
 * ANNOBY argument).
 *
 * PB: PACKETS or BYTES. Count number of packets or bytes.
 *
 * OFF: offset in packet where IP header starts
 *
 * RATIO: inspect 1 in n packets. 1 makes IPRateMonitor inspect every packet.
 *
 * THRESH: IPRateMonitor further splits a subnet if rate is over THRESH number
 * packets or bytes per second. Always give specify value as if RATIO were 1.
 *
 * MEMORY: How much memory can IPRateMonitor use in kilobytes? Set to 100 if
 * supplied argument < 100. 0 is unlimited memory.
 *
 * =h look (read)
 * Returns the rate of counted to and from a cluster of IP
 * addresses. The first printed line is the number of 'jiffies' that have past
 * since the last reset. There are 100 jiffies in one second.
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
 * Monitors packet rates for packets coming in on one port. Approximately 50% of
 * all packets is inspected. The memory usage is limited to 600K. When rate for
 * a network address (e.g. 18.26.*.*) exceeds 256 packets per second, start
 * monitor subnet or host addresses (e.g. 18.26.4.*). Keep packet per second
 * rate for the past 1 second. Annotate packet's rate annotations with rates for
 * the DST IP address.
 *
 * =a IPFlexMonitor
 */

#include "glue.hh"
#include "click_ip.h"
#include "element.hh"
#include "ewma.hh"
#include "vector.hh"

#define BYTES 4
#define MAX_SHIFT ((BYTES-1)*8)
#define MAX_COUNTERS 256

struct HalfSecondsTimer {
  static unsigned now()			{ return click_jiffies() >> 3; }
};

class IPRateMonitor : public Element {
public:
  IPRateMonitor();
  ~IPRateMonitor();

  const char *class_name() const		{ return "IPRateMonitor"; }
  const char *default_processing() const	{ return AGNOSTIC; }

  IPRateMonitor *clone() const;
  void notify_ninputs(int);  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  struct Stats;

  void update_alloced_mem(int m);
  void set_resettime()                          { _resettime = MyEWMA::now(); }
  void set_first(Stats *s)                      { _first = s; }
  void set_last(Stats *s)                       { _last = s; }

  // XXX: HACK!
  void set_prev(Stats *s)                       { _prev_deleted = s; }

  void push(int port, Packet *p);
  Packet *pull(int port);


private:
  typedef RateEWMAX<5, 10, HalfSecondsTimer> MyEWMA;
  
  // one for each address
  struct Counter {
    MyEWMA rev_rate;
    MyEWMA fwd_rate;
    Stats *next_level;
  };

  struct Stats {
    // Only used when leaf, not when parent.
    Counter *_parent;
    Stats *_prev, *_next;

    Counter* counter[MAX_COUNTERS];
    Stats(IPRateMonitor *m);
    // Stats(IPRateMonitor *m, const MyEWMA &, const MyEWMA &);
    ~Stats();
    void clear();

  private:
    IPRateMonitor *_rm;             // XXX: to access update_alloced_mem()
  };


#define COUNT_PACKETS 0
#define COUNT_BYTES 1
  unsigned char _pb;                // packets or bytes
  int _offset;                      // offset in packet
  int _thresh;                      // threshold, when to split

#define MEMMAX_MIN 100              // kbytes
  int _memmax;                      // max. memory usage
  int _ratio;                       // inspect 1 in how many packets?

  struct Stats *_base;              // first level stats
  long unsigned int _resettime;     // time of last reset
  int _alloced_mem;                 // total allocated memory
  struct Stats *_first, *_last;     // first and last element in linked list


  // XXX: HACK
  Stats *_prev_deleted;

  void update_rates(Packet *, bool forward);
  void update(IPAddress saddr, int val, Packet *p, bool forward);
  void forced_fold();
  void fold(int);
  void move_to_front(Stats *s);
  void prepend_to_front(Stats *s);

  void show_agelist(void);

  String print(Stats *s, String ip = "");

  void add_handlers();
  static String thresh_read_handler(Element *e, void *);
  static String look_read_handler(Element *e, void *);
  static String what_read_handler(Element *e, void *);
  static String mem_read_handler(Element *e, void *);
  static String memmax_read_handler(Element *e, void *);
  static int reset_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  static int memmax_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
};

//
// Dives in tables based on a and raises all rates by val.
//
inline void
IPRateMonitor::update(IPAddress saddr, int val,
		      Packet *p, bool forward)
{
  unsigned int addr = saddr.addr();
  struct Stats *s = _base;
  Counter *c = 0;
  int bitshift;
  bool newed = false;

  int now = MyEWMA::now();

  // find entry on correct level
  for (bitshift = 0; bitshift <= MAX_SHIFT; bitshift += 8) {
    unsigned char byte = (addr >> bitshift) & 0x000000ff;

    // Allocate Counter record if it doesn't exist yet.
    if(!(c = s->counter[byte])) {
      c = s->counter[byte] = new Counter;
      _alloced_mem += sizeof(Counter);
      newed = true;
      c->fwd_rate.initialize();
      c->rev_rate.initialize();
      c->next_level = 0;
    }

    if (forward)
      c->fwd_rate.update(now, val);
    else
      c->rev_rate.update(now, val);

    // Move this Stats struct to front of age list
    if(s != _first)
      move_to_front(s);
    if(!c->next_level)
      break;
    s = c->next_level;
  }

  // annotate src and dst rate for dst address
  int fwd_rate = c->fwd_rate.average(); 
  p->set_fwd_rate_anno(fwd_rate);
  int rev_rate = c->rev_rate.average(); 
  p->set_rev_rate_anno(rev_rate);

  // did value get larger than THRESH in the specified period?
  if (fwd_rate >= _thresh || rev_rate >= _thresh) {
    if (bitshift < MAX_SHIFT) {
      // Forcefully allocate next level
      while(!(c->next_level = new Stats(this))) {
        forced_fold();
      }
      newed = true;

      // Tell parent about newly created structure and prepend to list.
      c->next_level->_parent = c;
      prepend_to_front(c->next_level);
    }
  }

  // Did we allocate too much memory? Force it down by lowering the threshold
  if(newed && _memmax && (_alloced_mem > _memmax))
    forced_fold();
}


inline void
IPRateMonitor::forced_fold()
{
  for(int thresh = _thresh; _alloced_mem > _memmax; thresh <<= 1)
    fold(thresh);
}

//
// NB: Should not be called when already first in list!
//
inline void
IPRateMonitor::move_to_front(Stats *s)
{
  // s->_prev has valid value, because never called when s == _first.
  s->_prev->_next = s->_next;

  // Untangle next. Only executed when not first.
  if(s->_next)
    s->_next->_prev = s->_prev;
  else {
    _last = s->_prev;
    _last->_next = 0;
  }

  prepend_to_front(s);
}


// Move this stats to front of age list
inline void
IPRateMonitor::prepend_to_front(Stats *s)
{
  _first->_prev = s;
  s->_prev = 0;
  s->_next = _first;
  _first = s;
}


inline void
IPRateMonitor::update_rates(Packet *p, bool forward)
{
  click_ip *ip = (click_ip *) (p->data() + _offset);
  int val = (_pb == COUNT_PACKETS) ? 1 : ip->ip_len;
  if (forward)
    update(IPAddress(ip->ip_src), val, p, true);
  else
    update(IPAddress(ip->ip_dst), val, p, false);
}


inline void
IPRateMonitor::update_alloced_mem(int m)
{
  _alloced_mem += m;
}



#endif /* IPRATEMON_HH */
