#ifndef LOOKUPIPROUTE_HH
#define LOOKUPIPROUTE_HH

/*
 * =c
 * LookupIPRoute(DST1 MASK1 GW1 OUT1, DST2 MAS2 GW2 OUT2, ...)
 * =d
 * Input: IP packets (no ether header).
 * Expects a destination IP address annotation with each packet.
 * Looks up the address, sets the destination annotation to
 * the corresponding GW (if non-zero), and emits the packet
 * on the indicated OUTput.
 *
 * Each comma-separated argument is a route, specifying
 * a destination and mask, a gateway (zero means none),
 * and an output index.
 *
 * =e
 * This example delivers broadcasts and packets addressed to the local
 * host (18.26.4.24) to itself, packets to net 18.26.4 to the
 * local interface, and all others via gateway 18.26.4.1:
 *
 * = ... -> GetIPAddress(16) -> rt;
 * = rt :: LookupIPRoute(18.26.4.24  255.255.255.255 0.0.0.0 0,
 * =                 18.26.4.255 255.255.255.255 0.0.0.0 0,
 * =                 18.26.4.0   255.255.255.255 0.0.0.0 0,
 * =                 18.26.4.0 255.255.255.0 0.0.0.0 1,
 * =                 0.0.0.0 0.0.0.0 18.26.4.1 1);
 * = rt[0] -> ToLinux;
 * = rt[1] -> ... -> ToDevice(eth0);
 *
 * =n
 * Only static routes are allowed. If you need a dynamic routing
 * protocol such as RIP, run it at user-level and use
 * LookupIPRouteLinux.
 *
 * =a LookupIPRouteLinux
 */

#include "element.hh"
#include "iptable.hh"

class LookupIPRoute : public Element {
public:
  LookupIPRoute();
  ~LookupIPRoute();
  
  const char *class_name() const		{ return "LookupIPRoute"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  LookupIPRoute *clone() const;
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void push(int port, Packet *p);

private:

  IPTable _t;
  IPAddress _last_addr;
  IPAddress _last_gw;
  int _last_output;
  
};

#endif
