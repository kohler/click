#ifndef RANDOMLOSSAGE_HH
#define RANDOMLOSSAGE_HH
#include "element.hh"

/*
 * =c
 * RandomLossage(p)
 * =d
 * Drops each packet with probability p.
 *
 * If the element has two outputs, packets are sent to output
 * 1 rather than being dropped.
 *
 * /proc/click/xxx/active controls whether the element actually
 * drops packets.
 *
 * =a RandomBitErrors
 */

class RandomLossage : public Element {
  
  int _p_drop;			// out of 0xFFFF
  bool _on;
  int _drops;
  
 public:
  
  RandomLossage(int p_drop = -1, bool = true);
  
  const char *class_name() const		{ return "RandomLossage"; }
  void notify_noutputs(int);
  void processing_vector(Vector<int> &, int, Vector<int> &, int) const;
  
  int p_drop() const				{ return _p_drop; }
  bool on() const				{ return _on; }
  int drops() const				{ return _drops; }
  
  RandomLossage *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  void add_handlers(HandlerRegistry *);
  
  void push(int port, Packet *);
  Packet *pull(int port);
  
};

#endif
