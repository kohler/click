#ifndef CLICK_RANDOMLOSSAGE_HH
#define CLICK_RANDOMLOSSAGE_HH
#include <click/element.hh>
#include <click/atomic.hh>

/*
 * =c
 *
 * RandomLossage(P [, ACTIVE])
 *
 * =s dropping
 *
 * drops packets with some probability
 *
 * =deprecated RandomSample
 *
 * =d
 *
 * This element is deprecated. Use RandomSample(DROP P) instead.
 *
 * =a RandomSample */

class RandomLossage : public Element { public:
  
  RandomLossage();
  ~RandomLossage();
  
  const char *class_name() const		{ return "RandomLossage"; }
  RandomLossage *clone() const			{ return new RandomLossage; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
};

#endif
