#ifndef METER_HH
#define METER_HH
#include "element.hh"
#include "ewma.hh"

/*
 * =c
 * Meter(rate1, rate2, ..., rate<i>n</i>)
 * =d
 * Classifies packets based on how fast bytes are arriving. The configuration
 * string consists of one or more rate arguments. Each rate is measured in
 * bytes per second, and earlier rates in the list must be less than later
 * rates. A Meter element with <i>n</i> rate arguments will have
 * <i>n</i>+1 outputs. The Meter measures the incoming data rate using a
 * exponential weighted moving average, and sends packets out the
 * output corresponding to the current rate. (So if the rate is less than
 * rate1, packets will be sent on output 0; if it is >= rate1 but < rate2,
 * packets will be sent on output 1; and so on. If it is >= rate<i>n</i>,
 * packets will be sent on output <i>n</i>-1.)
 * =a PacketMeter
 * =a Shaper
 * =a PacketShaper
 */

class Meter : public Element { protected:

  EWMA _rate;

  int _meter1;
  int *_meters;
  int _nmeters;

  static String meters_read_handler(Element *, void *);

 public:
  
  Meter();
  ~Meter();
  
  const char *class_name() const		{ return "Meter"; }
  Processing default_processing() const	{ return PUSH; }
  
  int rate() const				{ return _rate.average(); }
  int rate_scale() const			{ return _rate.scale(); }
  
  Meter *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers(HandlerRegistry *);
  
  void push(int port, Packet *);
  
};

#endif
