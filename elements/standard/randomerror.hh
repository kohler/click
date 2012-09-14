#ifndef CLICK_RANDOMERROR_HH
#define CLICK_RANDOMERROR_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * RandomBitErrors(P [, KIND, ACTIVE])
 * =s basicmod
 * changes packet data with some probability
 * =d
 *
 * Change each bit in each packet with probability P. The KIND parameter
 * determines the kind of change. `flip' (the default) flips bits, `set' sets
 * bits to one, and `clear' sets bits to zero.
 *
 * RandomBitErrors can be active or inactive. It only changes bits when
 * active. It starts out active by default, but if you pass `false' for the
 * ACTIVE parameter, it will start out inactive.
 *
 * =h p_bit_error read/write
 * Returns or sets the P probability parameter.
 * =h error_kind read/write
 * Returns or sets the KIND parameter.
 * =h active read/write
 * Makes the element active or inactive.
 *
 * =a RandomSample */

class RandomBitErrors : public Element {

  int _p_error[9];		// out of 0xFFFF
  unsigned _p_bit_error;	// out of 0xFFFF
  int _kind;			// 0 clear, 1 set, 2 flip
  bool _on;

 public:

  RandomBitErrors() CLICK_COLD;

  const char *class_name() const		{ return "RandomBitErrors"; }
  const char *port_count() const		{ return PORTS_1_1; }

  unsigned p_bit_error() const			{ return _p_bit_error; }
  int kind() const				{ return _kind; }
  bool on() const				{ return _on; }
  void set_bit_error(unsigned);	// out of 0xFFFF

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }
  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
