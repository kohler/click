#ifndef PROBSPLITTER_HH
#define PROBSPLITTER_HH
#include "element.hh"

/*
 * =c
 * ProbSplitter(P)
 * =s classifies packets probabilistically
 * =d
 * 
 * ProbSplitter has two output ports. It will push a packet onto port 1 with P
 * probability. Otherwise, packet is pushed onto port 0.
 *
 * =e
 *   ProbSplitter(.2);
 * Pushes packets with 20% probability.
 *
 * =h prob read/write
 * sample probability
 *
 * =a Tee, RatedSplitter
 */

class ProbSplitter : public Element {
  
 public:
  
  ProbSplitter() : Element(1,2)			{}
  ~ProbSplitter() 				{}
  ProbSplitter *clone() const			{ return new ProbSplitter; }

  const char *class_name() const		{ return "ProbSplitter"; }
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


