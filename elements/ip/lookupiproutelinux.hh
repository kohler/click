#ifndef CLICK_LINUXIPLOOKUP_HH
#define CLICK_LINUXIPLOOKUP_HH
#include <click/element.hh>
#include <click/iptable.hh>
CLICK_DECLS

/*
 * =c
 * LinuxIPLookup(if0, if1, ..., ifN)
 * =s iproute
 * interface to Linux's routing table
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
 *   r : LinuxIPLookup(eth0, eth1)
 *   r[0] -> ... -> ToDevice(eth0)
 *   r[1] -> ... -> ToDevice(eth1)
 *   r[2] -> ICMPError(18.26.4.24, 3, 0) -> ...
 *
 * =a RadixIPLookup, DirectIPLookup, RangeIPLookup, StaticIPLookup,
 * LinearIPLookup, SortedIPLookup
 */

class LinuxIPLookup : public Element {
public:
  LinuxIPLookup() CLICK_COLD;
  ~LinuxIPLookup() CLICK_COLD;

  const char *class_name() const	{ return "LinuxIPLookup"; }
  const char *port_count() const	{ return "1/1-"; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  void push(int, Packet *);

private:
  int _nout; // number of named outputs. really one more.
  Vector<String> _out2devname; // dev name of each output.

  int init_routes(ErrorHandler *);
  bool lookup(IPAddress, IPAddress &, int &);

#ifdef CLICK_LINUXMODULE
  net_device **_out2dev; // dev ptr of each output.
#else
  IPTable _t;
#endif
};

CLICK_ENDDECLS
#endif
