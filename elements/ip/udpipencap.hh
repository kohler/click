#ifndef UDPIPENCAP_HH
#define UDPIPENCAP_HH

/*
 * =c
 * UDPIPEncap(SADDR, SPORT, DADDR, DPORT [, CHECKSUM?])
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
 * = UDPIPEncap(1234,1234,1)
 * =a Strip
 */

#include "element.hh"
#include "glue.hh"
#include "click_udp.h"

class UDPIPEncap : public Element {
public:
  UDPIPEncap();
  ~UDPIPEncap();
  
  const char *class_name() const	{ return "UDPIPEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  UDPIPEncap *clone() const;
  int configure(const String &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
private:
  struct in_addr _saddr;
  unsigned short _sport;
  struct in_addr _daddr;
  unsigned short _dport;
  bool _cksum;
  short _id;
};

#endif
