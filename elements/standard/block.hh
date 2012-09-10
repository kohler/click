#ifndef CLICK_BLOCK_HH
#define CLICK_BLOCK_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * Block(THRESH)
 * =s shaping
 * drops packets with high rate
 * =d
 * THRESH is an integer.
 *
 * Splits packets based on the dst rate annotation set by Monitor. If the
 * annotation is less or equal to THRESH, the packet is pushed on output 0,
 * otherwise on 1.
 *
 * Is THRESH is 0 then all packets are pushed on output 0.
 *
 * =e
 *   b :: Block(500);
 *
 *   ... -> Monitor(...) ->
 *   ... -> b[0] -> ...
 *   ... -> b[1] -> Discard;
 *
 * Discards all packets that are preceded by more than 500 siblings.
 *
 * =h thresh read/write
 * value of THRESH
 *
 * =a IPFlexMonitor
 */

class Block : public Element {

 public:

  Block() CLICK_COLD;

  const char *class_name() const		{ return "Block"; }
  const char *port_count() const		{ return "1/2"; }
  void add_handlers() CLICK_COLD;

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  // bool can_live_reconfigure() const		{ return true; }

  void push(int port, Packet *);
  // Packet *pull(int port);

 private:

  int _thresh;

  static int thresh_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh) CLICK_COLD;
  static String thresh_read_handler(Element *e, void *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
