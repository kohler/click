#ifndef PACKETSHAPER_HH
#define PACKETSHAPER_HH
#include "shaper.hh"

/*
 * =c
 * PacketShaper(RATE)
 * =d
 * PacketShaper is a pull element that allows a maximum of RATE packets per
 * second to pass through. It measures RATE using an exponential weighted
 * moving average.
 *
 * There are usually Queues both upstream and downstream
 * of Shaper elements.
 * =a Shaper
 * =a Meter
 * =a PacketMeter
 */

class PacketShaper : public Shaper {

 public:
  
  PacketShaper();
  
  const char *class_name() const                { return "PacketShaper"; }
  PacketShaper *clone() const;
  
  Packet *pull(int);
  
};

#endif
