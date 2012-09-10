#ifndef CLICK_BANDWIDTHMETER_HH
#define CLICK_BANDWIDTHMETER_HH
#include <click/element.hh>
#include <click/ewma.hh>
CLICK_DECLS

/*
 * =c
 * BandwidthMeter(RATE1, RATE2, ..., RATEI<n>)
 * =s shaping
 * classifies packet stream by arrival rate
 * =d
 *
 * Classifies packet stream based on the rate of packet arrival.  The rate
 * is measured in bytes per second using an exponential weighted moving
 * average.  (The related Meter element measures rates in packets per
 * second.)
 *
 * The configuration string consists of one or more RATE arguments.  Each
 * RATE is a bandwidth, such as "384 kbps".  Earlier
 * rates in the list must be less than later rates. A Meter with I<n> rate
 * arguments will have I<n>+1 outputs. It sends packets out the output
 * corresponding to the current rate. If the rate is less than RATE1
 * packets are sent to output 0; if it is >= RATE1 but < RATE2, packets are
 * sent to output 1; and so on. If it is >= RATEI<n>, packets are sent to
 * output I<n>.
 *
 * =e
 *
 * This configuration fragment drops the input stream when it is generating
 * more than 20,000 bytes per second.
 *
 *   ... -> m :: BandwidthMeter(20kBps) -> ...;
 *   m[1] -> Discard;
 *
 * =a Meter, BandwidthShaper, Shaper, RatedSplitter */

class BandwidthMeter : public Element { protected:

  RateEWMA _rate;

  unsigned _meter1;
  unsigned *_meters;
  int _nmeters;

  static String meters_read_handler(Element *, void *) CLICK_COLD;
  static String read_rate_handler(Element *, void *);

 public:

  BandwidthMeter() CLICK_COLD;
  ~BandwidthMeter() CLICK_COLD;

  const char *class_name() const		{ return "BandwidthMeter"; }
  const char *port_count() const		{ return "1/2-"; }
  const char *processing() const		{ return PUSH; }

  unsigned scaled_rate() const		{ return _rate.scaled_average(); }
  unsigned rate_scale() const		{ return _rate.scale(); }
  unsigned rate_freq() const		{ return _rate.epoch_frequency(); }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  void push(int port, Packet *);

};

CLICK_ENDDECLS
#endif
