#ifndef PACKETSHAPER_HH
#define PACKETSHAPER_HH
#include "shaper.hh"

/*
 * =c
 * PacketShaper(RATE)
 * =s shapes traffic to maximum rate (pkt/s)
 * V<packet scheduling>
 * =d
 * PacketShaper is a pull element that allows a maximum of RATE packets per
 * second to pass through. It measures RATE using an exponential weighted
 * moving average. Because the granularity of the timer used for calculating
 * the moving average is coarse, PacketShaper is not accurate for rates less
 * than 30000. Use PacketShaper2 instead for those rates.
 *
 * There are usually Queues both upstream and downstream
 * of Shaper elements.
 * =a Shaper, Meter, PacketMeter
 */

class PacketShaper : public Shaper {

 public:
  
  PacketShaper();
  
  const char *class_name() const                { return "PacketShaper"; }
  PacketShaper *clone() const;
  
  Packet *pull(int);
  
};

#endif
