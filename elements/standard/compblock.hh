#ifndef COMPBLOCK_HH
#define COMPBLOCK_HH
#include "element.hh"

/*
 * =c
 * CompareBlock(FWD_WEIGHT, REV_WEIGHT, THRESH)
 * =d
 * FWD_WEIGHT and REV_WEIGHT are integers
 *
 * Splits packets based on the rate annotation set by IPRateMonitor. If either
 * rate annotation is greater than THRESH and FWD_WEIGHT*fwd_rate_anno() >=
 * REV_WEIGHT*rev_rate_anno(), the packet is pushed on output 0, otherwise on
 * 1. By default, FWD_WEIGHT is 0, and REV_WEIGHT is 1 (all packets go to
 * output 0).
 *
 * =e
 * = b :: CompareBlock(5, 2);
 * if 5*fwd_rate >= 2*rev_rate AND fwd_rate or rev_rate > THRESH, send out
 * output 0.
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
 * =a Block
 * =a IPRateMonitor
 */

class CompareBlock : public Element {
  
 public:
  
  CompareBlock();
  CompareBlock *clone() const;

  const char *class_name() const		{ return "CompareBlock"; }
  const char *processing() const	        { return AGNOSTIC; }
  void add_handlers();
  
  int configure(const String &, ErrorHandler *);
  void push(int port, Packet *);

 private:

  int _fwd_weight;
  int _rev_weight;
  int _thresh;

  static int rev_weight_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  static int fwd_weight_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  static int thresh_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);

  static String rev_weight_read_handler(Element *e, void *);
  static String fwd_weight_read_handler(Element *e, void *);
  static String thresh_read_handler(Element *e, void *);
};

#endif


