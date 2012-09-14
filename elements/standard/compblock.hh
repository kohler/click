#ifndef CLICK_COMPBLOCK_HH
#define CLICK_COMPBLOCK_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * CompareBlock(FWD_WEIGHT, REV_WEIGHT, THRESH)
 * =s shaping
 * drops packets out of rate range
 * =d
 * FWD_WEIGHT, REV_WEIGHT, and THRESH are integers
 *
 * Splits packets based on the fwd_rate_anno and rev_rate_anno rate annotations
 * set by IPRateMonitor. If either annotation is greater than THRESH,
 * and FWD_WEIGHT*fwd_rate_anno > REV_WEIGHT*rev_rate_anno,
 * the packet is pushed on output 1, otherwise on output 0.
 *
 * =e
 *   b :: CompareBlock(5, 2, 100);
 * if (5*fwd_rate > 2*rev_rate) AND (fwd_rate > 100 or rev_rate > 100), send
 * packet out on output 1, otherwise on output 0.
 *
 * =h fwd_weight read/write
 * value of FWD_WEIGHT
 *
 * =h rev_weight read/write
 * value of REV_WEIGHT
 *
 * =h thresh read/write
 * value of THRESH
 *
 * =a Block, IPRateMonitor
 */

class CompareBlock : public Element { public:

  CompareBlock() CLICK_COLD;

  const char *class_name() const		{ return "CompareBlock"; }
  const char *port_count() const		{ return "1/2"; }
  void add_handlers() CLICK_COLD;

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  void push(int port, Packet *);

 private:

  int _fwd_weight;
  int _rev_weight;
  int _thresh;
  int _bad;

  static int rev_weight_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  static int fwd_weight_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  static int thresh_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);

  static String rev_weight_read_handler(Element *e, void *) CLICK_COLD;
  static String fwd_weight_read_handler(Element *e, void *) CLICK_COLD;
  static String thresh_read_handler(Element *e, void *) CLICK_COLD;
};

CLICK_ENDDECLS
#endif
