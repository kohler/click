#ifndef RRUDPIPENCAP_HH
#define RRUDPIPENCAP_HH

/*
 * =c
 * RoundRobinUDPIPEncap(SADDR SPORT DADDR DPORT [CHECKSUM?] [, ...])
 * =d
 * Encapsulates each incoming packet in a UDP/IP packet with source address
 * SADDR, source port SPORT, destination address DADDR, and destination port
 * DPORT. The UDP checksum is calculated if CHECKSUM? is true; it is true by
 * default.
 *
 * The RoundRobinUDPIPEncap element adds both a UDP header and an IP header.
 *
 * You can give as many arguments as you'd like. Each argument specifies a
 * single UDP/IP header. The element will cycle through the headers in
 * round-robin order.
 *
 * The Strip element can be used by the receiver to get rid of the
 * encapsulation header.
 * =e
 * = RoundRobinUDPIPEncap(1.0.0.1 1234 2.0.0.2 1234,
 * = 			  1.0.0.2 1093 2.0.0.2 1234)
 * =a Strip
 * =a UDPIPEncap */

#include "element.hh"
#include "glue.hh"
#include "click_udp.h"

class RoundRobinUDPIPEncap : public Element {

  struct Addrs {
    struct in_addr saddr;
    struct in_addr daddr;
    unsigned short sport;
    unsigned short dport;
    bool cksum;
    short id;
  };
  unsigned _naddrs;
  unsigned _pos;
  Addrs *_addrs;
  bool _aligned;

 public:
  
  RoundRobinUDPIPEncap();
  ~RoundRobinUDPIPEncap();
  
  const char *class_name() const	{ return "RoundRobinUDPIPEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  RoundRobinUDPIPEncap *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  Packet *simple_action(Packet *);
  
};

#endif
