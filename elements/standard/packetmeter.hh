#ifndef PACKETMETER_HH
#define PACKETMETER_HH
#include "meter.hh"

/*
 * =c
 * PacketMeter(rate1, rate2, ..., rate<i>n</i>)
 * =d
 * Classifies packets based on how fast they are arriving. The configuration
 * string consists of one or more rate arguments. Each rate is measured in
 * packets per second, and earlier rates in the list must be less than later
 * rates. A Meter element with <i>n</i> rate arguments will have
 * <i>n</i>+1 outputs. The Meter measures the incoming packet rate using a
 * exponential weighted moving average, and sends packets out the
 * output corresponding to the current rate. (So if the rate is less than
 * packets will be sent on output 0; if it is >= rate1 but < rate2, packets
 * will be sent on output 1; and so on. If it is >= rate<i>n</i>, packets
 * will be sent on output <i>n</i>.)
 * =a Meter
 * =a Shaper
 * =a PacketShaper
 */

class PacketMeter : public Meter {

 public:
  
  PacketMeter();
  
  const char *class_name() const		{ return "PacketMeter"; }
  PacketMeter *clone() const;
  
  void push(int port, Packet *);
  
};

#endif
