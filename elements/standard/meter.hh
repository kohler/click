#ifndef METER_HH
#define METER_HH
#include "element.hh"
#include "ewma.hh"

/*
 * =c
 * Meter(RATE1, RATE2, ..., RATE<i>n</i>)
 * =d
 *
 * Classifies packets based on the rate of packet arrival. The rate is
 * measured in bytes per second using an exponential weighted moving average.
 * (The related PacketMeter element measures rates in packets per second.)
 * 
 * The configuration string consists of one or more rate arguments. Earlier
 * rates in the list must be less than later rates. A Meter with <i>n</i> rate
 * arguments will have <i>n</i>+1 outputs. It sends packets out the output
 * corresponding to the current rate. If the rate is less than RATE1 packets
 * are sent to output 0; if it is >= RATE1 but < RATE2, packets are sent to
 * output 1; and so on. If it is >= RATE<i>n</i>, packets are sent to output
 * <i>n</i>.
 *
 * =e
 *
 * This configuration fragment drops the input stream when it is generating
 * more than 20,000 bytes per second.
 *
 * = ... -> m :: Meter(20000) -> ...;
 * = m[1] -> Discard;
 *
 * =a PacketMeter
 * =a Shaper
 * =a PacketShaper */

class Meter : public Element { protected:

  RateEWMA _rate;

  int _meter1;
  int *_meters;
  int _nmeters;

  static String meters_read_handler(Element *, void *);

 public:
  
  Meter();
  ~Meter();
  
  const char *class_name() const		{ return "Meter"; }
  const char *processing() const	{ return PUSH; }
  
  int rate() const				{ return _rate.average(); }
  int rate_scale() const			{ return _rate.scale; }
  int rate_freq() const				{ return _rate.freq(); }
  
  Meter *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();
  
  void push(int port, Packet *);
  
};

#endif
