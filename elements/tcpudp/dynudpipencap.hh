#ifndef DYNUDPIPENCAP_HH
#define DYNUDPIPENCAP_HH

/*
 * =c
 * DynamicUDPIPEncap(SADDR, SPORT, DADDR, DPORT [, CHECKSUM? [, INTERVAL]])
 * =s encapsulation, UDP
 * encapsulates packets in static UDP/IP headers
 * =d
 * Encapsulates each incoming packet in a UDP/IP packet
 * with source address SADDR, source port SPORT,
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

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/click_udp.h>

class DynamicUDPIPEncap : public Element {

  struct in_addr _saddr;
  struct in_addr _daddr;
  unsigned short _sport;
  unsigned short _dport;
  bool _cksum : 1;
  bool _aligned : 1;
  uatomic32_t _id;
  uatomic32_t _count;
  unsigned _interval;

 public:
  
  DynamicUDPIPEncap();
  ~DynamicUDPIPEncap();
  
  const char *class_name() const	{ return "DynamicUDPIPEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  DynamicUDPIPEncap *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
};

#endif
