#ifndef CLICK_TCPREFLECTOR_HH
#define CLICK_TCPREFLECTOR_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * TCPReflector()
 * =d
 * Pretend to be a TCP server; emit a plausible reply packet
 * to each incoming TCP/IP packet. Maintains no state, so
 * should be very fast.
 * =e
 * FromDevice(eth1, 0)
 *   -> Strip(14)
 *   -> CheckIPHeader()
 *   -> IPFilter(allow tcp && dst host 1.0.0.77 && dst port 99 && not src port 99, deny all)
 *   -> Print(x)
 *   -> TCPReflector()
 *   -> Print(y)
 *   -> EtherEncap(0x0800, 1:1:1:1:1:1, 00:03:47:07:E9:94)
 *   -> ToDevice(eth1);
 */

class TCPReflector : public Element {
 public:
  
  TCPReflector();
  ~TCPReflector();
  
  const char *class_name() const		{ return "TCPReflector"; }
  const char *processing() const		{ return AGNOSTIC; }
  TCPReflector *clone() const { return new TCPReflector; }

  Packet *simple_action(Packet *);
  Packet *TCPReflector::tcp_input(Packet *xp);
};

CLICK_ENDDECLS
#endif
