#ifndef IPRATEMON_HH
#define IPRATEMON_HH

/*
 * =c
 * IPRateMonitor(DS, PB, OFF, THRESH/PERIOD, T1, ..., T4)
 * =d
 *
 * Monitors network traffic rates, much like IPFlexMonitor. Can monitor either
 * packet or byte rate for either dst IP addresses or src IP addresses. Can
 * keep rates over up to four time periods. When the rate for a particular IP
 * network address exceeds the threshold, rates will then be kept for host or
 * subnet addresses within that network.
 *
 * DS: SRC or DST. Look at src or dst IP address.
 *
 * PB: PACKETS or BYTES. Count number of packets or bytes.
 *
 * OFF: offset in packet where IP header starts
 * 
 * THRESH/PERIOD: THRESH number of packets or bytes per PERIOD seconds will
 *                IPRateMonitor to monitor subnet or host addresses within a
 *                network.
 *
 * T1, T2, etc.: Instructs IPRateMonitor to keep track of rates over the given
 *               time periods.
 *
 * =h look (read)
 * Returns the rate of counted to/from a cluster of IP addresses. The first
 * printed line is the number of 'jiffies' that have past since the last reset.
 * There are 100 jiffies in one second.
 *
 * =h srcdst (read)
 * Returns value of DS.
 *
 * =h what (read)
 * Returns value of PB
 *
 * =h thresh (read-write)
 * When read, returns THRESH/PERIOD. When written, expects THRESH/PERIOD.
 *
 * =h rates (read)
 * Returns rates over T1, T2, T3, T4
 *
 * =e
 * Example: 
 *
 * IPRateMonitor(DST, PACKETS, 0, 100000/10, 30 60)
 *
 * Monitors packet rates on dst IP addresses. When rate for a network address
 * (e.g. 18.26.*.*) exceeds 100000 packets per 10 seconds, start monitor
 * subnet or host addresses (e.g. 18.26.4.*). For each host or network
 * address, keeps packet rates over the last 30 and 60 seconds.
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
#define MAX_NRATES   5 // first rate is hidden

class IPRateMonitor : public Element { public:

  IPRateMonitor();
  ~IPRateMonitor();

  const char *class_name() const		{ return "IPRateMonitor"; }
  const char * default_processing() const	{ return AGNOSTIC; }
  
  void uninitialize() 				{ destroy(_base); }
  int configure(const String &conf, ErrorHandler *errh);

  IPRateMonitor *clone() const;
  Packet *simple_action(Packet *);

private:

  unsigned char _sd;
#define SRC 0
#define DST 1

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
    EWMA2 values[MAX_NRATES];
    struct _stats *next_level;
    int last_update;
  };

  struct _stats {
    struct _counter counter[MAX_COUNTERS];
  };

  int _thresh;
  unsigned int _rates[MAX_NRATES];  // number of rates to keep
  unsigned short _no_of_rates;        // equals _rates.size()
  struct _stats *_base;
  long unsigned int _resettime;       // time of last reset

  void set_resettime();
  bool set_thresh(String str);
  void update(IPAddress a, int val);

  String print(_stats *s, String ip = "");
  void clean(_stats *s);
  void destroy(_stats *s);

  void add_handlers();
  static String thresh_read_handler(Element *e, void *);
  static String look_read_handler(Element *e, void *);
  static String what_read_handler(Element *e, void *);
  static String srcdst_read_handler(Element *e, void *);
  static String rates_read_handler(Element *e, void *);

  static int thresh_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  static int reset_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
};


//
// Dives in tables based on a and raises all rates by val.
//
inline void
IPRateMonitor::update(IPAddress a, int val)
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
      for(int i = 0; i < _no_of_rates; i++)
        c->values[i].update(val);
      s = c->next_level;
    }
    else
      break;
  }

  // is vector allocated already?
  if(!c->flags) {
    for(int i = 0; i < _no_of_rates; i++)
      c->values[i].initialize(_rates[i]);
    c->flags = INIT;
  }

  // update values.
  for(int i = 0; i < _no_of_rates; i++)
    c->values[i].update(val);

  // did value get larger than THRESH in the specified period?
  if(c->values[0].average() >= _thresh)
    if(bitshift < MAX_SHIFT) {
      c->flags |= SPLIT;
      struct _stats *tmp = new struct _stats;
      clean(tmp);
      c->next_level = tmp;
    }
}

#endif /* IPRATEMON_HH */

