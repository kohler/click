#ifndef CLICK_METER_HH
#define CLICK_METER_HH
#include "bandwidthmeter.hh"
CLICK_DECLS

/*
 * =c
 * Meter(RATE1, RATE2, ..., RATEI<n>)
 * =s shaping
 * classifies packet stream by rate (pkt/s)
 * =d
 *
 * Classifies packets based on the rate of packet arrival. The rate is
 * measured in packets per second using an exponential weighted moving
 * average. (The related BandwidthMeter element measures rates in bytes per
 * second.)
 *
 * The configuration string consists of one or more rate arguments. Earlier
 * rates in the list must be less than later rates. A Meter with I<n> rate
 * arguments will have I<n>+1 outputs. It sends packets out the output
 * corresponding to the current rate. If the rate is less than RATE1 packets
 * are sent to output 0; if it is >= RATE1 but < RATE2, packets are sent to
 * output 1; and so on. If it is >= RATEI<n>, packets are sent to output I<n>.
 *
 * =n
 *
 * The entire packet stream is sent to the output corresponding to the current
 * rate. If you would like the packet stream to be split, with at most RATE1
 * packets per second being sent out the first output and the remainder being
 * sent to the second output, check out RatedSplitter.
 *
 * =e
 *
 * This configuration fragment drops the input stream when it is generating
 * more than 10,000 packets per second.
 *
 *   ... -> m :: Meter(10000) -> ...;
 *   m[1] -> Discard;
 *
 * =a BandwidthMeter, RatedSplitter, Shaper, BandwidthShaper,
 * RatedUnqueue, BandwidthRatedUnqueue */

class Meter : public BandwidthMeter { public:

  Meter();

  const char *class_name() const		{ return "Meter"; }

  void push(int port, Packet *);

};

CLICK_ENDDECLS
#endif
