#ifndef IPENCAP_HH
#define IPENCAP_HH

/*
 * =c
 * IPEncap(PROTOCOL, SADDR, DADDR)
 * =s
 * encapsulates packets in static IP header
 * V<encapsulation>
 * =d
 * Encapsulates each incoming packet in an IP packet with protocol
 * PROTOCOL, source address SADDR, and destination address DADDR.
 * This is most useful for IP-in-IP encapsulation.
 *
 * The packet's TTL and TOS header fields are set to 250 and 0 respectively.
 * Its destination annotation is set to DADDR.
 *
 * The Strip element can be used by the receiver to get rid
 * of the encapsulation header.
 *
 * =e
 * Wraps packets in an IP header specifying IP protocol 4
 * (IP-in-IP), with source 18.26.4.24 and destination 140.247.60.147:
 *
 *   IPEncap(4, 18.26.4.24, 140.247.60.147)
 *
 * =a UDPIPEncap, Strip
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/click_ip.h>

class IPEncap : public Element {
  
  int _ip_p; // IP protocol number field.
  struct in_addr _ip_src;
  struct in_addr _ip_dst;
#ifdef __KERNEL__
  bool _aligned;
#endif

  short _id;

 public:
  
  IPEncap();
  ~IPEncap();
  
  const char *class_name() const		{ return "IPEncap"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  IPEncap *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
  
};

#endif
