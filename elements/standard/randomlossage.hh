#ifndef RANDOMLOSSAGE_HH
#define RANDOMLOSSAGE_HH
#include <click/element.hh>

/*
 * =c
 * RandomLossage(P [, ACTIVE])
 * =s dropping
 * drops packets with some probability
 * =d
 * Drops each packet with probability P.
 * If the element has two outputs, packets are sent to output
 * 1 rather than being dropped.
 *
 * RandomLossage can be active or inactive. It only drops packets when active.
 * It starts out active by default, but if you pass `false' for the ACTIVE
 * parameter, it will start out inactive.
 *
 * =h p_drop read/write
 * Returns or sets the P probability parameter.
 * =h active read/write
 * Makes the element active or inactive.
 * =h drops read-only
 * Returns the number of packets dropped.
 *
 * =a RandomBitErrors */

class RandomLossage : public Element {
  
  int _p_drop;			// out of 0xFFFF
  bool _on;
  int _drops;
  
 public:
  
  RandomLossage();
  ~RandomLossage();
  
  const char *class_name() const		{ return "RandomLossage"; }
  const char *processing() const		{ return "a/ah"; }
  void notify_noutputs(int);
  
  int p_drop() const				{ return _p_drop; }
  bool on() const				{ return _on; }
  int drops() const				{ return _drops; }
  
  RandomLossage *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  void add_handlers();
  
  void push(int port, Packet *);
  Packet *pull(int port);
  
};

#endif
