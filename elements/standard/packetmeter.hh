#ifndef PACKETMETER_HH
#define PACKETMETER_HH
#include "meter.hh"

/*
 * =c
 * PacketMeter(RATE1, RATE2, ..., RATEI<n>)
 * =s
 * classifies packet stream by rate (pkt/s)
 * V<classification>
 * =d
 *
 * Classifies packets based on the rate of packet arrival. The rate is
 * measured in packets per second using an exponential weighted moving
 * average. (The related Meter element measures rates in bytes per second.)
 * 
 * The configuration string consists of one or more rate arguments. Earlier
 * rates in the list must be less than later rates. A PacketMeter with
 * I<n> rate arguments will have I<n>+1 outputs. It sends packets out
 * the output corresponding to the current rate. If the rate is less than
 * RATE1 packets are sent to output 0; if it is >= RATE1 but < RATE2, packets
 * are sent to output 1; and so on. If it is >= RATEI<n>, packets are sent
 * to output I<n>.
 *
 * =e
 *
 * This configuration fragment drops the input stream when it is generating
 * more than 10,000 packets per second.
 *
 *   ... -> m :: PacketMeter(10000) -> ...;
 *   m[1] -> Discard;
 *
 * =a Meter, Shaper, PacketShaper */

class PacketMeter : public Meter {

 public:
  
  PacketMeter();
  
  const char *class_name() const		{ return "PacketMeter"; }
  PacketMeter *clone() const;
  
  void push(int port, Packet *);
  
};

#endif
