#ifndef MONITOR_HH
#define MONITOR_HH

/*
 * =c
 * Monitor("SRC"|"DST", MAX [, VAR1, VAR2, ..., VARn])
 * =d
 * Input: IP packets (no ether header).
 *
 * Monitors traffic by counting the number of packets going to/coming from
 * (clusters of) IP addresses.
 *
 * The first argument is either "SRC" or "DST". SRC will instruct Monitor to
 * collect statistics based on the source address in the IP header. DST will
 * instruct Monitor to collect statistics based on the destination address.
 *
 * MAX is a value denoting an amount of packets per second. Normally, Monitor
 * clusters IP addresses based on the first byte of the IP address. When the
 * number of packets going to/coming from a cluster exceeds MAX, it is split
 * based on the second byte, and so on.
 *
 * The number of inputs for Monitor equals n in VARn. Each VARx is related to
 * one input. Packets coming in on input x will cause the value of that src/dst
 * cluster to be changed with the value of VARx. If no VAR is supplied, then
 * Monitor has one input with a weight of 1.
 *
 * Monitor should be used together with Classifier to count packets with
 * specific features.
 *
 * =h look (read)
 * Returns the number of packets counted to/from a cluster of IP addresses.
 * 
 * =h max (read-write)
 * Used to read/write MAX value.
 *
 * =h srcdst (read-write)
 * Used to read/write SRC/DST value.
 *
 * = reset (write)
 * Resets all entries to zero.
 *
 * =e
 *
 * For example,
 *
 * = c :: Classifier(SYN, SYN-ACK);
 * =
 * = ... -> c;
 *
 * = m :: Monitor(DST, 10, 1, -2);
 * =
 * = c[0] -> [0]m -> ...
 * = c[1] -> [1]m -> ...
 *
 * makes m count packets based on the destination IP address. For every
 * SYN-packet, the value is raised by 1, for every SYN-ACK packet the value is
 * lowered by 2.
 *
 * (Classifier has been simplified in this example)
 *
 * =a Classifier
 * =a Funnel
 */

#include "glue.hh"
#include "element.hh"
#include "monitor.hh"
#include "vector.hh"


class Monitor : public Element {
public:
  Monitor();
  ~Monitor();
  
  const char *class_name() const		{ return "Monitor"; }
  const char * default_processing() const	{ return AGNOSTIC; }
  int configure(const String &conf, ErrorHandler *errh);
  Monitor *clone() const;
  
  void push(int port, Packet *p);

private:

#define SPLIT   0x0001

  struct _stats;

  struct _counter {
    unsigned char flags;
    union {
      unsigned int value;
      struct _stats *next_level;
    };
  };

  struct _stats {
    struct _counter counter[256];
  };

  int _src_dst;
#define SRC 0
#define DST 1

  int _max;
  Vector<int> _inputs;
  struct _stats *_base;

  void clean(_stats *s, int value = 0, bool recurse = false);
  void update(IPAddress a, int val);
  String print(_stats *s, String ip = "");

  static String srcdst_rhandler(Element *e, void *);
  static int srcdst_whandler(const String &conf, Element *e, void *, ErrorHandler *errh);
  static String max_rhandler(Element *e, void *);
  static int max_whandler(const String &conf, Element *e, void *, ErrorHandler *errh);
  static String look_handler(Element *e, void *);
  static int reset_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
  void add_handlers();
};

#endif /* MONITOR_HH */
