#ifndef RATEDSPLITTER_HH
#define RATEDSPLITTER_HH
#include "element.hh"
#include "ewma.hh"

/*
 * =c
 * RatedSplitter(R)
 * =s
 * splits flow of packets at specified rate
 * V<classification>
 * =d
 * 
 * RatedSplitter has two output ports. It will push packets out on port 1 at
 * rate R. Otherwise, packets are pushed out on port 0.
 *
 * =e
 *   RatedSplitter(2000);
 * Split packets on port 1 at 2000 packets per second.
 *
 *   elementclass RatedSampler {
 *     input -> rs :: RatedSplitter(2000);
 *     rs [0] -> [0] output;
 *     rs [1] -> t :: Tee;
 *     t [0] -> [0] output;
 *     t [1] -> [1] output;
 *   };
 * In the above example, RatedSampler is a compound element that 
 * samples input packets at 2000 packets per second.
 *
 * =h rate read/write
 * rate of splitting
 *
 * =a Tee, ProbSplitter, PacketShaper2
 */

class RatedSplitter : public Element {
  
 public:
  
  RatedSplitter() : Element(1,2)		{}
  ~RatedSplitter() 				{}
  RatedSplitter *clone() const			{ return new RatedSplitter; }

  const char *class_name() const		{ return "RatedSplitter"; }
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


