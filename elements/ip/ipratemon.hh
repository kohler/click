#ifndef IPRATEMON_HH
#define IPRATEMON_HH

/*
 * =c
 * IPRateMonitor(N, PB, OFF, THRESH, ANNOBY [,ANNO_PORT0, ANNO_PORT1, ... ])
 * =d
 *
 * Monitors network traffic rates. Can monitor either packet or byte rate (per
 * second) to and from an address. When the to or from rate for a particular
 * address exceeds the threshold, rates will then be kept for host or subnet
 * addresses within that address. May update each packet's dst_rate_anno and
 * src_rate_anno with the rates for dst or src IP address (specified by the
 * ANNOBY argument).
 *
 * N (default to 1): number of input/output port pairs.
 *
 * PB (default to PACKETS): PACKETS or BYTES. Count number of packets or bytes.
 *
 * OFF (default to 0): offset in packet where IP header starts
 *
 * THRESH (default to 10): IPRateMonitor further splits a subnet if rate is
 * over THRESH number packets or bytes per second.
 *
 * ANNOBY (default to DST): DST or SRC. Annotate by DST or SRC IP address.
 *
 * ANNO_PORTX (default to false): true or false. if true, annotate packet
 * going through port X.
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
 * IPRateMonitor(1, PACKETS, 0, 256, DST, true);
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

#if IPVERSION == 4
#define BYTES 4
#endif
#define MAX_SHIFT ((BYTES-1)*8)
#define MAX_COUNTERS 256
#define MAX_PORT_PAIRS 16

struct HalfSecondsTimer {
  static unsigned now()			{ return click_jiffies() >> 4; }
};

class IPRateMonitor : public Element { public:

  IPRateMonitor();
  ~IPRateMonitor();

  const char *class_name() const		{ return "IPRateMonitor"; }
  const char * default_processing() const	{ return AGNOSTIC; }
  
  void uninitialize() 				{ _base->clear(); }
  int configure(const String &conf, ErrorHandler *errh);

  IPRateMonitor *clone() const;
  void push(int port, Packet *p);
  Packet *pull(int port);

private:

  typedef RateEWMAX<4, 10, HalfSecondsTimer> MyEWMA;
  
  unsigned char _pb;
#define COUNT_PACKETS 0
#define COUNT_BYTES 1

  int _offset;
  bool _annobydst;
  bool _anno[MAX_PORT_PAIRS];

  // one for each input
  struct _inp {
    int change;
    unsigned char srcdst;
  };

  // one for each address
  struct Stats;
  struct Counter {
    MyEWMA dst_rate;
    MyEWMA src_rate;
    struct Stats *next_level;
  };

  struct Stats {
    struct Counter counter[MAX_COUNTERS];
    Stats();
    ~Stats();
    void clear();
  };

  int _thresh;
  struct Stats *_base;
  long unsigned int _resettime;       // time of last reset

  void set_resettime();
  Packet *update_rates(Packet *, int port);
  void update(IPAddress dstaddr, IPAddress srcaddr, 
              int val, Packet *p, int port);

  String print(Stats *s, String ip = "");

  void add_handlers();
  static String thresh_read_handler(Element *e, void *);
  static String look_read_handler(Element *e, void *);
  static String what_read_handler(Element *e, void *);

  static int reset_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
};

//
// Dives in tables based on a and raises all rates by val.
//
inline void
IPRateMonitor::update(IPAddress dstaddr, IPAddress srcaddr, 
                      int val, Packet *p, int port)
{
  unsigned int saddr = dstaddr.saddr();
  unsigned int dst = true;
  int now = MyEWMA::now();

 restart:
  struct Stats *s = _base;
  Counter *c = 0;
  int bitshift;

  // find entry on correct level
  for (bitshift = 0; bitshift <= MAX_SHIFT && s; bitshift += 8) {
    unsigned char byte = (saddr >> bitshift) & 0x000000ff;
    c = &(s->counter[byte]);
    if (dst) 
      c->dst_rate.update(now, val);
    else 
      c->src_rate.update(now, val);
    s = c->next_level;
  }

  // annotate src and dst rate for dst address
  int cr;
  if (dst) {
    cr = (c->dst_rate.average()*CLICK_HZ) >> c->dst_rate.scale;
    if (_anno[port] && _annobydst) {
      p->set_dst_rate_anno(cr);
      p->set_src_rate_anno
	((c->src_rate.average()*CLICK_HZ)>>c->src_rate.scale);
    }
  }
  else {
    cr = (c->src_rate.average()*CLICK_HZ) >> c->src_rate.scale;
    if (_anno[port] && !_annobydst) {
      p->set_dst_rate_anno
	((c->dst_rate.average()*CLICK_HZ)>>c->dst_rate.scale);
      p->set_src_rate_anno(cr);
    }
  }

  // did value get larger than THRESH in the specified period?
  if (cr >= _thresh) {
    if (bitshift < MAX_SHIFT)
      c->next_level = new Stats;
  }

  if (dst) {
    dst = false;
    saddr = srcaddr.saddr();
    goto restart;
  }
}

inline Packet *
IPRateMonitor::update_rates(Packet *p, int port)
{
  IPAddress dstaddr, srcaddr;
  click_ip *ip = (click_ip *) (p->data() + _offset);
  int val = (_pb == COUNT_PACKETS) ? 1 : ip->ip_len;
  
  dstaddr = IPAddress(ip->ip_dst);
  srcaddr = IPAddress(ip->ip_src);
  update(dstaddr, srcaddr, val, p, port);

  return p;
}


#endif /* IPRATEMON_HH */

