#ifndef UDPENCAP_HH
#define UDPENCAP_HH

/*
 * =c
 * UDPEncap(sport, dport, cksum)
 * =d
 * Encapsulates each incoming packet in an UDP packet with source port sport
 * and destination port dport. UDP checksum is done if cksum is 1. By default,
 * sport = 1234, dport = 1234, cksum = 1.
 *
 * The Strip element can be used by the receiver to get rid of the
 * encapsulation header.
 * =e
 * = UDPEncap(1234,1234,1)
 * =a Strip
 */

#include "element.hh"
#include "glue.hh"
#include "click_udp.h"

class UDPEncap : public Element {
public:
  UDPEncap();
  ~UDPEncap();
  
  const char *class_name() const	{ return "UDPEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  UDPEncap *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
  
private:
  unsigned short _cksum;
  unsigned short _sport;
  unsigned short _dport;
};

#endif
