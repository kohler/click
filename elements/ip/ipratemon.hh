#ifndef IPRATEMON_HH
#define IPRATEMON_HH

/*
 * =c
 * IPRateMonitor(PB, OFF, THRESH, REFRESH)
 * =d
 *
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
 * THRESH: IPRateMonitor further splits a subnet if rate is
 * over THRESH number packets or bytes per second.
 *
 * REFRESH: Clean up per how many packets?
 *
 * =h look (read)
 * Returns the rate of counted to and from a cluster of IP addresses. The
 * first printed line is the number of 'jiffies' that have past since the last
 * reset. There are 100 jiffies in one second.
 *
 * =h thresh (read)
 * Returns THRESH.
 *
 * =h reset (write)
 * When written, resets all rates.
 *
 * =e
 * Example: 
 *
 * IPRateMonitor(PACKETS, 0, 256);
 *
 * Monitors packet rates for packets coming in on one port. When rate for a
 * network address (e.g. 18.26.*.*) exceeds 1000 packets per second, start
 * monitor subnet or host addresses (e.g. 18.26.4.*). Keep packet per second
 * rate for the past 1 second. Annotate packet's rate annotations with rates
 * for the DST IP address.
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

  void update_alloced_mem(int m)		{ _alloced_mem += m; }
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

    Counter counter[MAX_COUNTERS];
    Stats(IPRateMonitor *m);
    Stats(IPRateMonitor *m, const MyEWMA &, const MyEWMA &);
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
  int _refresh;                     // refresh rate

  struct Stats *_base;              // first level stats
  long unsigned int _resettime;     // time of last reset
  int _alloced_mem;                 // total allocated memory
  struct Stats *_first, *_last;     // first and last element in linked list
  int _packet_counter;

  // XXX: HACK
  Stats *_prev_deleted;

  void update_rates(Packet *, bool forward);
  void update(IPAddress saddr, int val, Packet *p, bool forward);
  void fold();
  void move_to_front(Stats *s);
  void prepend_to_front(Stats *s);

  void show_agelist(void);

  String print(Stats *s, String ip = "");

  void add_handlers();
  static String thresh_read_handler(Element *e, void *);
  static String look_read_handler(Element *e, void *);
  static String what_read_handler(Element *e, void *);
  static String mem_read_handler(Element *e, void *);
  static String refresh_read_handler(Element *e, void *);
  static int reset_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  static int refresh_write_handler
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
  int now = MyEWMA::now();

  struct Stats *s = _base;
  Counter *c = 0;
  int bitshift;

  // find entry on correct level
  unsigned char byte;
  for (bitshift = 0; bitshift <= MAX_SHIFT; bitshift += 8) {
    byte = (addr >> bitshift) & 0x000000ff;
    c = &(s->counter[byte]);
    if (forward)
      c->fwd_rate.update(now, val);
    else
      c->rev_rate.update(now, val);

    // Move to front of list
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
      c->next_level = new Stats(this, c->fwd_rate, c->rev_rate);
      if(!c->next_level) {
        // Clean up and try again
      }

      // Tell parent about newly created structure and prepend to list.
      c->next_level->_parent = c;
      prepend_to_front(c->next_level);
    }
  }
}


inline void
IPRateMonitor::move_to_front(Stats *s)
{
  // Untangle prev
  if(s->_prev)
    s->_prev->_next = s->_next;
  else {
    // Already first in list
    assert(s == _first);
    return;
  }

  // Untangle next. Only executed when not first.
  if(s->_next)
    s->_next->_prev = s->_prev;
  else {
    // This stats was last in age list, make preceding last.
    assert(s == _last);
    _last = s->_prev;
    _last->_next = 0;
  }

  prepend_to_front(s);
}


// Move this stats to front of age list
inline void
IPRateMonitor::prepend_to_front(Stats *s)
{
  assert(_first->_prev == 0);
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

  if(_packet_counter++ == _refresh) {
    fold();
    _packet_counter = 0;
  }
}

#endif /* IPRATEMON_HH */



// CODE TO USE WHEN PARENTS ARE NOT IN AGE LIST
#if 0
      // The parent of this newly created Stats thingy (i.e., s) is now in front
      // of list (i.e., s == _first), but it has become a parent. We have to
      // kick it out and replace it for its child.
      if((c->next_level->_next = s->_next))
        c->next_level->_next->_prev = c->next_level;
      else
        _last = c->next_level;
      _first = c->next_level;
      assert(s->_prev == 0);
      s->_next = 0; // s->_prev is already 0
#endif
