#ifndef RATEDSAMPLER_HH
#define RATEDSAMPLER_HH
#include "element.hh"
#include "ewma.hh"

/*
 * =c
 * RatedSampler(R)
 * =d
 * 
 * RatedSampler has two output ports. It will always push packets out on port
 * 0. It will also push packets out on port 1 at rate R. 
 *
 * =e
 * = RatedSampler(2000);
 * Sample packets on port 1 at 2000 packets per second.
 *
 * =h rate read/write
 * sample rate
 *
 * =a Tee
 * =a ProbSampler
 * =a PacketShaper2
 * =a PacketShaper
 */

class RatedSampler : public Element {
  
 public:
  
  RatedSampler() : Element(1,2)			{}
  ~RatedSampler() 				{}
  RatedSampler *clone() const			{ return new RatedSampler; }

  const char *class_name() const		{ return "RatedSampler"; }
  const char *processing() const	        { return PUSH; }
  void add_handlers();
 
  int configure(const Vector<String> &, ErrorHandler *);
  void push(int port, Packet *);
  
  int get_rate() const				{ return _meter; }
  void set_rate(int r);

 private:

  unsigned _meter;
  unsigned _ugap;
  unsigned _total;
  struct timeval _start;

};

#endif


