#ifndef COMPBLOCK_HH
#define COMPBLOCK_HH
#include "element.hh"

/*
 * =c
 * CompareBlock(DST_WEIGHT, SRC_WEIGHT)
 * =d
 * DST_WEIGHT and SRC_WEIGHT are integers
 *
 * Splits packets based on the rate annotation set by IPRateMonitor. If
 * DST_WEIGHT*dst_rate_anno() <= SRC_WEIGHT*src_rate_anno(), the packet is
 * pushed on output 0, otherwise on 1. By default, DST_WEIGHT is 0, and
 * SRC_WEIGHT is 1 (all packets go to output 0).
 *
 * =e
 * = b :: CompareBlock(5,2);
 * if 5 * dst_rate > 2 * src_rate, drop packet.
 * 
 * =h dst_weight read/write
 * value of DST_WEIGHT
 * 
 * =h src_weight read/write
 * value of SRC_WEIGHT
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
  int initialize(ErrorHandler *);
  void push(int port, Packet *);

 private:

  int _dst_weight;
  int _src_weight;

  static int src_weight_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  static int dst_weight_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);

  static String src_weight_read_handler(Element *e, void *);
  static String dst_weight_read_handler(Element *e, void *);
};

#endif


