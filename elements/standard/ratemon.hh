#ifndef RATEMON_HH
#define RATEMON_HH

/*
 * =c
 * RateMonitor(DS, OFF, PB, THRESH/PERIOD, T1, T2, ...)
 * =d
 *
 * See Monitor.
 *
 * DS: "SRC" or "DST". Look at src or dst IP address
 * OFF: offset in packet where IP header starts
 * PB: count packets or bytes
 * THRESH/PERIOD: THRESH per PERIOD seconds causes a split
 * T1, T2: Instructs RateMonitor to keep track of last Tx seconds
 *
 * =h look (read)
 * Returns the rate of counted to/from a cluster of IP addresses. The first
 * printed line is the number of 'jiffies' that have past since the last reset.
 * There are 100 jiffies in one second.
 *
 * =h srcdst (read)
 *
 *
 * =h what (read)
 *
 *
 * =h thresh (read-write)
 *
 * =e
 *
 * =a Monitor
 */
#include "glue.hh"
#include "click_ip.h"
#include "element.hh"
#include "ewma.hh"
#include "vector.hh"


class RateMonitor : public Element {
public:
  RateMonitor();
  ~RateMonitor();
  
  const char *class_name() const		{ return "RateMonitor"; }
  const char * default_processing() const	{ return AGNOSTIC; }
  int configure(const String &conf, ErrorHandler *errh);
  RateMonitor *clone() const;
  // void push(int port, Packet *p);
  Packet *simple_action(Packet *);

private:

// XXX: Is this somewhere defined already?
#if IPVERSION == 4
#define BYTES 4
#endif

#define MAX_SHIFT ((BYTES-1)*8)

  unsigned char _sd;
#define SRC 0
#define DST 1

  unsigned char _pb;
#define COUNT_PACKETS 0
#define COUNT_BYTES 1

  unsigned char _offset;

  // One of these associated with each input.
  struct _inp {
    int change;
    unsigned char srcdst;
  };

  struct _stats;

  // For each (cluster of) IP address(es).
  struct _counter {
    unsigned char flags;
#define SPLIT     0x0001

    union {
      Vector<EWMA> *values;
      struct _stats *next_level;
    };

    int last_update;
  };

  struct _stats {
    struct _counter counter[256];
  };

  int _thresh;
  Vector<unsigned int> _rates;          // value associated with each input
  unsigned short _no_of_rates;          // equals _rates.size()
  struct _stats *_base;                 // base struct for monitoring
  long unsigned int _resettime;         // time of last reset

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

  static int thresh_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
  static int reset_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
};

#endif /* MONITOR_HH */
