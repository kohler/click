#ifndef PROBSAMPLER_HH
#define PROBSAMPLER_HH
#include "element.hh"

/*
 * =c
 * ProbSampler(P)
 * =d
 * 
 * ProbSampler has two output ports. It will always push packets out on port
 * 0, but there is a P chance that it will also push the packet out on port 1. 
 *
 * =e
 * = ProbSampler(.2);
 * Samples packets with 20% probability.
 *
 * =h prob read/write
 * sample probability
 *
 * =a Tee
 * =a RatedSampler
 */

class ProbSampler : public Element {
  
 public:
  
  ProbSampler() : Element(1,2)			{}
  ~ProbSampler() 				{}
  ProbSampler *clone() const			{ return new ProbSampler; }

  const char *class_name() const		{ return "ProbSampler"; }
  const char *processing() const	        { return PUSH; }
  void add_handlers();
  
  int configure(const Vector<String> &, ErrorHandler *);
  void push(int port, Packet *);

  void set_p(unsigned p)			{ _p = p; }
  unsigned get_p() const			{ return _p; }

 private:

  unsigned _p;
};

#endif


