#ifndef MONITOR_HH
#define MONITOR_HH

/*
 * =c
 * Monitor("SRC"|"DST", MAX, VAR1, VAR2, ..., VARN)
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
 * The number of inputs and outputs for Monitor equals N in VARN. Each VARx
 * is related to one input and one output. 
 *
 * Monitor should be used together with Classifier to count packets with
 * specific features.
 *
 * =h look (read)
 * Returns ...
 * 
 * =h max (read-write)
 * Used to change MAX value.
 *
 * = reset (write)
 * Resets all entries to zero.
 *
 * =e
 *
 * For example,
 *
 * = c :: Classifier(SYN, NON-SYN);
 * =
 * = ... -> c;
 * =
 * = c[0] -> Monitor(DST, 10, 1);
 * = c[1] -> ...
 *
 * makes Monitor count the number of SYN packets going to which destinations. If
 * the number of SYN packets going to one cluster exceeds 10 per second, it is
 * split.
 *
 * =a Classifier
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
  struct _counter {
    unsigned char flags;
    unsigned int value;
  };


  struct _stats {
    struct _counter counter[256];
  };

  struct _base {
    int change;
    struct _stats *stats;
  };

  Vector<_base *> bases;

  void clean(_stats *s);
  void update(IPAddress a);
  void createbase(int change = 1);
};

#endif /* MONITOR_HH */
