#ifndef RANDOMUDPIPENCAP_HH
#define RANDOMUDPIPENCAP_HH

/*
 * =c
 * RandomUDPIPEncap(SADDR SPORT DADDR DPORT PROB [CHECKSUM?] [, ...])
 * =d
 * Encapsulates each incoming packet in a UDP/IP packet with source address
 * SADDR, source port SPORT, destination address DADDR, and destination port
 * DPORT. The UDP checksum is calculated if CHECKSUM? is true; it is true by
 * default.
 *
 * PROB gives the relative chance of this argument be used over others.
 *
 * The RandomUDPIPEncap element adds both a UDP header and an IP header.
 *
 * You can a maximum of 16 arguments. Each argument specifies a
 * single UDP/IP header. The element will randomly pick one argument. The
 * relative probabilities are determined by PROB.
 *
 * The Strip element can be used by the receiver to get rid of the
 * encapsulation header.
 * =e
 *   RandomUDPIPEncap(1.0.0.1 1234 2.0.0.2 1234 1 1,
 *   		      1.0.0.2 1093 2.0.0.2 1234 2 1)
 * 
 * Will send about twice as much UDP/IP packets with 1.0.0.2 as its source
 * address than packets with 1.0.0.1 as its source address.
 *
 * =a Strip, UDPIPEncap, RoundRobinUDPIPEncap
 */

#include "element.hh"
#include "glue.hh"
#include "click_udp.h"

class RandomUDPIPEncap : public Element {

  struct Addrs {
    struct in_addr saddr;
    struct in_addr daddr;
    unsigned short sport;
    unsigned short dport;
    bool cksum;
    short id;
  };
  unsigned _naddrs;
  Addrs *_addrs;
  unsigned _total_prob;
  bool _aligned;

  struct Rand {
    int n;
    Addrs *a;
  };
  struct Rand _randoms[16];
  short _no_of_addresses;

 public:
  
  RandomUDPIPEncap();
  ~RandomUDPIPEncap();
  
  const char *class_name() const	{ return "RandomUDPIPEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  RandomUDPIPEncap *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  Packet *simple_action(Packet *);
  
};

#endif
