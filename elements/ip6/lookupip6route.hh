#ifndef LOOKUPIP6ROUTE_HH
#define LOOKUPIP6ROUTE_HH

/*
 * =c
 * LookupIP6Route(DST1 MASK1 GW1 OUT1, DST2 MAS2 GW2 OUT2, ...)
 * =s
 * V<IPv6>
 * =d
 * Input: IP6 packets (no ether header).
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
 * host (::1261:027d) to itself, all others via gateway ::ffff:c0a8:1:
 *
 *   ... -> GetIP6Address(24) -> rt;
 *   rt :: LookupIP6Route(::ffff:1261:027d ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ::0 0,
 *	                  ::0 ::0 ::ffff:c0a8:1 1);
 *   rt[0] -> ToLinux;
 *   rt[1] -> ... -> ToDevice(eth0);
 *
 * =n
 * Only static routes are allowed. If you need a dynamic routing
 * protocol such as RIP, run it at user-level and use
 * LookupIPRouteLinux.
 *
 * =a LookupIPRouteLinux
 */

#include "element.hh"
#include "ip6table.hh"

class LookupIP6Route : public Element {
public:
  LookupIP6Route();
  ~LookupIP6Route();
  
  const char *class_name() const		{ return "LookupIP6Route"; }
  const char *processing() const		{ return AGNOSTIC; }
  LookupIP6Route *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void push(int port, Packet *p);

private:

  IP6Table _t;

  IP6Address _last_addr;
  IP6Address _last_gw;
  int _last_output;

#ifdef IP_RT_CACHE2
  IPAddress _last_addr2;
  IPAddress _last_gw2;
  int _last_output2;
#endif
  
};

#endif
