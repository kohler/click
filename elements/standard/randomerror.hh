#ifndef RANDOMERROR_HH
#define RANDOMERROR_HH
#include "element.hh"

/*
 * =c
 * RandomBitErrors(p, how)
 * =d
 * Flip each bit in each packet with probability p.
 *
 * If how is set or clear, do that to each bit instead of flipping it.
 *
 * /proc/click/xxx/active controls whether the element actually
 * creates errors.
 *
 * =a RandomLossage
 */

class RandomBitErrors : public Element {
  
  int _p_error[9];		// out of 0xFFFF
  unsigned _p_bit_error;	// out of 0xFFFF
  int _kind;			// 0 clear, 1 set, 2 flip
  bool _on;
  
 public:
  
  RandomBitErrors();
  RandomBitErrors(const RandomBitErrors &);
  
  const char *class_name() const		{ return "RandomBitErrors"; }
  Processing default_processing() const	{ return AGNOSTIC; }

  unsigned p_bit_error() const			{ return _p_bit_error; }
  int kind() const				{ return _kind; }
  bool on() const				{ return _on; }
  void set_bit_error(unsigned);	// out of 0xFFFF
  
  RandomBitErrors *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  void add_handlers(HandlerRegistry *);
  
  Packet *simple_action(Packet *);
  
};

#endif
