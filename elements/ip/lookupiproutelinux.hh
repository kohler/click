#ifndef LOOKUPIPROUTELINUX_HH
#define LOOKUPIPROUTELINUX_HH

/*
 * =c
 * LookupIPRouteLinux(if0, if1, ..., ifN)
 * =s interface to Linux's routing table
 * V<classification>
 * =d
 * Looks up each packet's destination annotation address in the
 * Linux routing table. Replaces the annotation with the
 * routing table entry's gateway field (if non-zero).
 * Emits the packet on an output that depends on the Linux
 * interface device mentioned in the routing table entry:
 * if the device name is the Ith configuration argument,
 * the element sends the packet on output I (zero origin).
 *
 * If the packet can't be routed, the element emits it on
 * the output with number equal to the number of configuration arguments.
 * A packet can't be routed if there is no matching routing
 * table entry, or if the device mentioned in the Linux
 * routing table isn't mentioned in the configuration.
 *
 * If run outside the kernel, the element reads the routing table
 * just once (at startup) from /proc/net/route.
 *
 * =e
 *   r : LookupIPRouteLinux(eth0, eth1)
 *   r[0] -> ... -> ToDevice(eth0)
 *   r[1] -> ... -> ToDevice(eth1)
 *   r[2] -> ICMPError(18.26.4.24, 3, 0) -> ...
 */

#include "element.hh"
#include "iptable.hh"

class LookupIPRouteLinux : public Element {
public:
  LookupIPRouteLinux();
  ~LookupIPRouteLinux();
  
  const char *class_name() const	{ return "LookupIPRouteLinux"; }
  const char *processing() const	{ return AGNOSTIC; }
  LookupIPRouteLinux *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void push(int, Packet *);

private:
  int _nout; // number of named outputs. really one more.
  Vector<String> _out2devname; // dev name of each output.

  int init_routes(ErrorHandler *);
  bool lookup(IPAddress, IPAddress &, int &);

#ifdef __KERNEL__
  struct device **_out2dev; // dev ptr of each output.
#endif

#ifndef __KERNEL__
  IPTable _t;
#endif
};

#endif
