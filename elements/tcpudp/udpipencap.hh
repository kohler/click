#ifndef CLICK_UDPIPENCAP_HH
#define CLICK_UDPIPENCAP_HH

/*
 * =c
 * UDPIPEncap(SADDR, SPORT, DADDR, DPORT [, CHECKSUM?])
 * =s encapsulation, UDP
 * encapsulates packets in static UDP/IP headers
 * =d
 * Encapsulates each incoming packet in a UDP/IP packet with source address
 * SADDR, source port SPORT, destination address DADDR, and destination port
 * DPORT. The UDP checksum is calculated if CHECKSUM? is true; it is true by
 * default.
 *
 * The UDPIPEncap element adds both a UDP header and an IP header.
 *
 * The Strip element can be used by the receiver to get rid of the
 * encapsulation header.
 * =e
 *   UDPIPEncap(1.0.0.1, 1234, 2.0.0.2, 1234)
 * =a Strip, IPEncap
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/click_udp.h>

class UDPIPEncap : public Element { public:

  UDPIPEncap();
  ~UDPIPEncap();
  
  const char *class_name() const	{ return "UDPIPEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  UDPIPEncap *clone() const;
  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);

 private:

  struct in_addr _saddr;
  struct in_addr _daddr;
  unsigned short _sport;
  unsigned short _dport;
  bool _cksum : 1;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  bool _aligned : 1;
#endif
  uatomic32_t _id;

};

#endif
