#ifndef CLICK_BANDWIDTHMETER_HH
#define CLICK_BANDWIDTHMETER_HH
#include <click/element.hh>
#include <click/ewma.hh>

/*
 * =c
 * BandwidthMeter(RATE1, RATE2, ..., RATEI<n>)
 * =s classification
 * classifies packet stream by arrival rate
 * =d
 *
 * Classifies packet stream based on the rate of packet arrival. The rate is
 * measured in bytes per second using an exponential weighted moving average.
 * (The related Meter element measures rates in packets per second.)
 * 
 * The configuration string consists of one or more rate arguments. Earlier
 * rates in the list must be less than later rates. A Meter with I<n> rate
 * arguments will have I<n>+1 outputs. It sends packets out the output
 * corresponding to the current rate. If the rate is less than RATE1 packets
 * are sent to output 0; if it is >= RATE1 but < RATE2, packets are sent to
 * output 1; and so on. If it is >= RATEI<n>, packets are sent to output
 * I<n>.
 *
 * =e
 *
 * This configuration fragment drops the input stream when it is generating
 * more than 20,000 bytes per second.
 *
 *   ... -> m :: BandwidthMeter(20000) -> ...;
 *   m[1] -> Discard;
 *
 * =a Meter, BandwidthShaper, Shaper, RatedSplitter */

class BandwidthMeter : public Element { protected:

  RateEWMA _rate;

  unsigned _meter1;
  unsigned *_meters;
  int _nmeters;

  static String meters_read_handler(Element *, void *);

 public:
  
  BandwidthMeter();
  ~BandwidthMeter();
  
  const char *class_name() const		{ return "BandwidthMeter"; }
  const char *processing() const		{ return PUSH; }
  
  unsigned rate() const				{ return _rate.average(); }
  unsigned rate_scale() const			{ return _rate.scale; }
  unsigned rate_freq() const			{ return _rate.freq(); }
  
  BandwidthMeter *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();
  
  void push(int port, Packet *);
  
};

#endif
