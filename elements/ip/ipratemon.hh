#ifndef IPRATEMON_HH
#define IPRATEMON_HH

/*
 * =c
 * IPRateMonitor(PB, OFF, THRESH)
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

class IPRateMonitor : public Element { public:

  IPRateMonitor();
  ~IPRateMonitor();

  const char *class_name() const		{ return "IPRateMonitor"; }
  const char *default_processing() const	{ return AGNOSTIC; }

  IPRateMonitor *clone() const;
  void notify_ninputs(int);  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void update_alloced_mem(int m)		{ _alloced_mem += m; }

  void push(int port, Packet *p);
  Packet *pull(int port);

 private:

  typedef RateEWMAX<5, 10, HalfSecondsTimer> MyEWMA;
  
  unsigned char _pb;
#define COUNT_PACKETS 0
#define COUNT_BYTES 1

  int _offset;

  // one for each address
  struct Stats;
  struct Counter {
    MyEWMA rev_rate;
    MyEWMA fwd_rate;
    Stats *next_level;
  };

  struct Stats {
    Counter counter[MAX_COUNTERS];
    Stats(IPRateMonitor *m);
    Stats(IPRateMonitor *m, const MyEWMA &, const MyEWMA &);
    ~Stats();
    void clear();
  private:
    IPRateMonitor *_rm;
  };

  int _thresh;
  struct Stats *_base;
  long unsigned int _resettime;       // time of last reset

  int _alloced_mem;

  void set_resettime();
  void update_rates(Packet *, bool forward);
  void update(IPAddress saddr, int val, Packet *p, bool forward);

  String print(Stats *s, String ip = "");

  void add_handlers();
  static String thresh_read_handler(Element *e, void *);
  static String look_read_handler(Element *e, void *);
  static String what_read_handler(Element *e, void *);
  static String mem_read_handler(Element *e, void *);

  static int reset_write_handler
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
  for (bitshift = 0; bitshift <= MAX_SHIFT; bitshift += 8) {
    unsigned char byte = (addr >> bitshift) & 0x000000ff;
    c = &(s->counter[byte]);
    if (forward) {
      c->fwd_rate.update(now, val);
    } else {
      c->rev_rate.update(now, val);
    }

    if(!(s = c->next_level))
      break;
  }

  // annotate src and dst rate for dst address
  int fwd_rate = c->fwd_rate.average(); 
  p->set_fwd_rate_anno(fwd_rate);
  int rev_rate = c->rev_rate.average(); 
  p->set_rev_rate_anno(rev_rate);
  /*
  click_chatter("ipratemon (%s -> %s) %d >> %d",
              IPAddress(((click_ip *)(p->data() + _offset))->ip_src.s_addr).s().cc(),
              IPAddress(((click_ip *)(p->data() + _offset))->ip_dst.s_addr).s().cc(),
              fwd_rate, rev_rate);
  */

  // did value get larger than THRESH in the specified period?
  if (fwd_rate >= _thresh || rev_rate >= _thresh) {
    if (bitshift < MAX_SHIFT) {
      c->next_level = new Stats(this, c->fwd_rate, c->rev_rate);
      if(!c->next_level) {
        // XXX: Clean up
      }
    }
  }
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

#endif /* IPRATEMON_HH */
