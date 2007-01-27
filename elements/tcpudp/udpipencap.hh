#ifndef CLICK_UDPIPENCAP_HH
#define CLICK_UDPIPENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/udp.h>
CLICK_DECLS

/*
 * =c
 * UDPIPEncap(SADDR, SPORT, DADDR, DPORT [, CHECKSUM?])
 * =s udp
 * encapsulates packets in static UDP/IP headers
 * =d
 * Encapsulates each incoming packet in a UDP/IP packet with source address
 * SADDR, source port SPORT, destination address DADDR, and destination port
 * DPORT. The UDP checksum is calculated if CHECKSUM? is true; it is true by
 * default.
 *
 * As a special case, if DADDR is "DST_ANNO", then the destination address
 * is set to the incoming packet's destination address annotation.
 *
 * The UDPIPEncap element adds both a UDP header and an IP header.
 *
 * The Strip element can be used by the receiver to get rid of the
 * encapsulation header.
 * =e
 *   UDPIPEncap(1.0.0.1, 1234, 2.0.0.2, 1234)
 * =a Strip, IPEncap
 */

class UDPIPEncap : public Element { public:

  UDPIPEncap();
  ~UDPIPEncap();
  
  const char *class_name() const	{ return "UDPIPEncap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const     { return true; }

  Packet *simple_action(Packet *);

 private:

  struct in_addr _saddr;
  struct in_addr _daddr;
  uint16_t _sport;
  uint16_t _dport;
  bool _cksum : 1;
  bool _use_dst_anno : 1;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  bool _aligned : 1;
#endif
  atomic_uint32_t _id;

};

CLICK_ENDDECLS
#endif
