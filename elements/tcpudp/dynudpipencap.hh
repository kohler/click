#ifndef CLICK_DYNUDPIPENCAP_HH
#define CLICK_DYNUDPIPENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/udp.h>
CLICK_DECLS

/*
 * =c
 * DynamicUDPIPEncap(SRC, SPORT, DST, DPORT [, CHECKSUM, INTERVAL])
 * =s udp
 * encapsulates packets in dynamic UDP/IP headers
 * =d
 * Encapsulates each incoming packet in a UDP/IP packet
 * with source address SRC, source port SPORT,
 * destination address DADDR, and destination port
 * DPORT. The UDP checksum is calculated if CHECKSUM? is
 * true; it is true by default. SPORT and DPORT are
 * incremented by 1 every INTERVAL number of packets. By
 * default, INTERVAL is 0, which means do not increment.
 * This is the same element as UDPIPEncap, except for
 * the INTERVAL functionality.
 *
 * The DynamicUDPIPEncap element adds both a UDP header
 * and an IP header.
 *
 * The Strip element can be used by the receiver to get
 * rid of the encapsulation header.
 * =e
 *   DynamicUDPIPEncap(1.0.0.1, 1234, 2.0.0.2, 1234, 1, 10)
 * =a Strip, IPEncap, UDPIPEncap
 */

class DynamicUDPIPEncap : public Element {

  struct in_addr _saddr;
  struct in_addr _daddr;
  uint16_t _sport;
  uint16_t _dport;
  bool _cksum : 1;
#ifdef CLICK_LINUXMODULE
  bool _aligned : 1;
#endif
  atomic_uint32_t _id;
  atomic_uint32_t _count;
  unsigned _interval;

 public:

  DynamicUDPIPEncap() CLICK_COLD;
  ~DynamicUDPIPEncap() CLICK_COLD;

  const char *class_name() const	{ return "DynamicUDPIPEncap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *flags() const		{ return "A"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
