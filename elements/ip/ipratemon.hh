#ifndef IPRATEMON_HH
#define IPRATEMON_HH

/*
 * =c
 * IPRateMonitor(PB, OFF, THRESH, T)
 * =d
 *
 * Monitors network traffic rates. Can monitor either packet or byte rate (per
 * second) to and from an address. When the to or from rate for a particular
 * address exceeds the threshold, rates will then be kept for host or subnet
 * addresses within that address.
 *
 * PB: PACKETS or BYTES. Count number of packets or bytes.
 *
 * OFF: offset in packet where IP header starts
 * 
 * THRESH: IPRateMonitor further splits a subnet if rate is over THRESH number
 *         packets or bytes per second.
 *
 * T: duration (in seconds) which the rate is kept for (e.g. last 60 seconds).
 *
 * =h look (read)
 * Returns the rate of counted to and from a cluster of IP addresses. The
 * first printed line is the number of 'jiffies' that have past since the last
 * reset. There are 100 jiffies in one second.
 *
 * =h what (read)
 * Returns value of PB
 *
 * =h thresh (read)
 * Returns THRESH.
 *
 * =h period (read)
 * Returns T.
 * 
 * =h reset (write)
 * When written, resets all rates.
 *
 * =e
 * Example: 
 *
 * IPRateMonitor(PACKETS, 0, 256, 1);
 *
 * Monitors packet rates. When rate for a network address (e.g. 18.26.*.*)
 * exceeds 1000 packets per second, start monitor subnet or host addresses
 * (e.g. 18.26.4.*). Keep packet per second rate for the past 1 second.
 *
 * =a IPFlexMonitor
 */

#include "glue.hh"
#include "click_ip.h"
#include "element.hh"
#include "ewma2.hh"
#include "vector.hh"

#if IPVERSION == 4
#define BYTES 4
#endif
#define MAX_SHIFT ((BYTES-1)*8)
#define MAX_COUNTERS 256

class IPRateMonitor : public Element { public:

  IPRateMonitor();
  ~IPRateMonitor();

  const char *class_name() const		{ return "IPRateMonitor"; }
  const char * default_processing() const	{ return AGNOSTIC; }
  
  void uninitialize() 				{ destroy(_base); }
  int configure(const String &conf, ErrorHandler *errh);

  IPRateMonitor *clone() const;
  Packet *simple_action(Packet *);
  void push(int port, Packet *p);
  Packet *pull(int port);

private:

  unsigned char _pb;
#define COUNT_PACKETS 0
#define COUNT_BYTES 1

  unsigned char _offset;

  // one for each input
  struct _inp {
    int change;
    unsigned char srcdst;
  };

  // one for each address
  struct _stats;
  struct _counter {
    unsigned char flags;
#define CLEAN    0x0000
#define INIT     0x0001
#define SPLIT    0x0010
    EWMA2 dst_rate;
    EWMA2 src_rate;
    int last_update;
    struct _stats *next_level;
  };

  struct _stats {
    struct _counter counter[MAX_COUNTERS];
  };

  int _period;
  int _thresh;
  struct _stats *_base;
  long unsigned int _resettime;       // time of last reset

  void set_resettime();
  void update(IPAddress a, bool dst, int val, Packet *p);

  String print(_stats *s, String ip = "");
  void clean(_stats *s);
  void destroy(_stats *s);

  void add_handlers();
  static String thresh_read_handler(Element *e, void *);
  static String look_read_handler(Element *e, void *);
  static String what_read_handler(Element *e, void *);
  static String srcdst_read_handler(Element *e, void *);
  static String period_read_handler(Element *e, void *);

  static int reset_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
};

//
// Dives in tables based on a and raises all rates by val.
//
inline void
IPRateMonitor::update(IPAddress a, bool dst, int val, Packet *p)
{
  unsigned int saddr = a.saddr();

  struct _stats *s = _base;
  struct _counter *c = NULL;
  int bitshift;

  // find entry on correct level
  for(bitshift = 0; bitshift <= MAX_SHIFT; bitshift += 8) {
    unsigned char byte = (saddr >> bitshift) & 0x000000ff;
    c = &(s->counter[byte]);

    if(c->flags & SPLIT) {
      if (dst) {
	c->dst_rate.update(val);
        c->last_update = c->dst_rate.now();
      }
      else {
	c->src_rate.update(val);
        c->last_update = c->src_rate.now();
      }
      s = c->next_level;
    }
    else
      break;
  }

  // is vector allocated already?
  if(!c->flags) {
    c->dst_rate.initialize(_period);
    c->src_rate.initialize(_period);
    c->flags = INIT;
  }

  // update values.
  if (dst) {
    c->dst_rate.update(val);
    c->last_update = c->dst_rate.now();
  }
  else {
    c->src_rate.update(val);
    c->last_update = c->src_rate.now();
  }

  // write annotation
  int sr = (c->src_rate.average()*CLICK_HZ) >> c->src_rate.scale();
  int dr = (c->dst_rate.average()*CLICK_HZ) >> c->dst_rate.scale();

  if (dst) 
    p->set_dst_rate_anno(dr);
  else
    p->set_src_rate_anno(sr);

  // did value get larger than THRESH in the specified period?
  if(sr >= _thresh || dr >= _thresh) {
    if(bitshift < MAX_SHIFT) {
      c->flags |= SPLIT;
      struct _stats *tmp = new struct _stats;
      clean(tmp);
      c->next_level = tmp;
    }
  }
}

#endif /* IPRATEMON_HH */

