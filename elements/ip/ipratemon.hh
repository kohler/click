#ifndef CLICK_IPRATEMON_HH
#define CLICK_IPRATEMON_HH
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/vector.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

/*
 * =c
 * IPRateMonitor(TYPE, RATIO, THRESH [, MEMORY, ANNO])
 * =s ipmeasure
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
 * TYPE: PACKETS or BYTES. Count number of packets or bytes.
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

class Spinlock;

class IPRateMonitor : public Element {
public:

    enum {
	stability_shift = 5,
	scale = 10
    };

    struct EWMAParameters : public FixedEWMAXParameters<stability_shift, scale> {
	enum {
	    rate_count = 2
	};

	static unsigned epoch() {
	    return click_jiffies() >> 3;
	}

	static unsigned epoch_frequency() {
	    return CLICK_HZ >> 3;
	}
    };


    IPRateMonitor() CLICK_COLD;
    ~IPRateMonitor() CLICK_COLD;

  const char *class_name() const		{ return "IPRateMonitor"; }
  const char *port_count() const		{ return "1-2/1-2"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;

  void set_resettime() {
      _resettime = EWMAParameters::epoch();
  }
  void set_anno_level(unsigned addr, unsigned level, unsigned when);

  void push(int port, Packet *p);
  Packet *pull(int port);

  int llrpc(unsigned, void *);

  typedef RateEWMAX<EWMAParameters> MyEWMA;
  struct Stats; struct Counter;	// so they can find one another

  struct Counter {
    // fwd_and_rev_rate.average[0] is forward rate
    // fwd_and_rev_rate.average[1] is reverse rate
    MyEWMA fwd_and_rev_rate;
    Stats *next_level;
    unsigned anno_this;
      Counter()
	  : next_level(0), anno_this(0) {
      }
      Counter(const MyEWMA &ewma)
	  : fwd_and_rev_rate(ewma), next_level(0), anno_this(0) {
      }
  };

  struct Stats {
    // one Stats for each subnet
    enum { MAX_COUNTERS = 256 };

    Counter *_parent;               // equals NULL for _base->_parent
    Stats *_prev, *_next;           // to maintain age-list

    Counter* counter[MAX_COUNTERS];
    Stats(IPRateMonitor *m);
    ~Stats() CLICK_COLD;

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
  void update_alloced_mem(ssize_t m)            { _alloced_mem += m; }

private:

  enum { MAX_SHIFT = 24, PERIODIC_FOLD_INIT = 8192, MEMMAX_MIN = 100 };
  // every n packets, a fold() is done

  bool _count_packets;		// packets or bytes
  bool _anno_packets;		// annotate packets?
  int _thresh;			// threshold, when to split
  size_t _memmax;		// max. memory usage
  unsigned int _ratio;		// inspect 1 in how many packets?
  Spinlock* _lock;		// synchronize handlers and update

  Stats *_base;			// first level stats
  long unsigned int _resettime;     // time of last reset
  size_t _alloced_mem;		// total allocated memory
  Stats *_first, *_last;	// first and last element in age list
  // HACK! For interaction between fold() and ~Stats()
  Stats *_prev_deleted, *_next_deleted;

  void update_rates(Packet *, bool, bool);
  void update(unsigned, int, Packet *, bool, bool);
  void forced_fold();
  void fold(int);
  Counter *make_counter(Stats *, unsigned char, MyEWMA *);

  void show_agelist(void);

  String print(Stats *s, String ip = "");

  void add_handlers() CLICK_COLD;
  static String look_read_handler(Element *e, void *) CLICK_COLD;
  static String what_read_handler(Element *e, void *) CLICK_COLD;
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
  Stats *s = _base;
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
  Stats *s = _base;
  Counter *c = 0;
  unsigned now = EWMAParameters::epoch();
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

  int fwd_rate = c->fwd_and_rev_rate.scaled_average(0);
  int rev_rate = c->fwd_and_rev_rate.scaled_average(1);
  int freq = EWMAParameters::epoch_frequency();
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

  if(now - prev_fold_time >= EWMAParameters::epoch_frequency()) {
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

CLICK_ENDDECLS
#endif /* CLICK_IPRATEMON_HH */
