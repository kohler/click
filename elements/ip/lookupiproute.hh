#ifndef LOOKUPIPROUTE_HH
#define LOOKUPIPROUTE_HH

/*
 * =c
 * LookupIPRoute(DST1/MASK1 [GW1] OUT1, DST2/MASK2 [GW2] OUT2, ...)
 * =s simple static IP routing table
 * V<classification>
 * =d
 * Input: IP packets (no ether header).
 * Expects a destination IP address annotation with each packet.
 * Looks up the address, sets the destination annotation to
 * the corresponding GW (if specified), and emits the packet
 * on the indicated OUTput.
 *
 * Each comma-separated argument is a route, specifying
 * a destination and mask; optionally, a gateway IP address;
 * and an output index.
 *
 * =e
 * This example delivers broadcasts and packets addressed to the local
 * host (18.26.4.24) to itself, packets to net 18.26.4 to the
 * local interface, and all others via gateway 18.26.4.1:
 *
 *   ... -> GetIPAddress(16) -> rt;
 *   rt :: LookupIPRoute(18.26.4.24/32 0,
 *                       18.26.4.255/32 0,
 *                       18.26.4.0/32 0,
 *                       18.26.4.0/24 1,
 *                       0.0.0.0/0 18.26.4.1 1);
 *   rt[0] -> ToLinux;
 *   rt[1] -> ... -> ToDevice(eth0);
 *
 * =n
 * Only static routes are allowed. If you need a dynamic routing
 * protocol such as RIP, run it at user-level and use
 * LookupIPRouteLinux or LookupIPRoute2.
 *
 * =a LookupIPRoute2, LookupIPRouteLinux
 */

#include "element.hh"
#include "iptable.hh"

#define IP_RT_CACHE2 1

class LookupIPRoute : public Element {
public:
  LookupIPRoute();
  ~LookupIPRoute();
  
  const char *class_name() const		{ return "LookupIPRoute"; }
  const char *processing() const		{ return AGNOSTIC; }
  LookupIPRoute *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void push(int port, Packet *p);

private:

  IPTable _t;

  IPAddress _last_addr;
  IPAddress _last_gw;
  int _last_output;

#ifdef IP_RT_CACHE2
  IPAddress _last_addr2;
  IPAddress _last_gw2;
  int _last_output2;
#endif
  
};

#endif
