#ifndef IPENCAP_HH
#define IPENCAP_HH

/*
 * =c
 * IPEncap(PROTOCOL, SADDR, DADDR)
 * =d
 * Encapsulates each incoming packet in an IP packet with protocol
 * PROTOCOL, source address SADDR, and destination address DADDR.
 * This is most useful for IP-in-IP encapsulation.
 *
 * If the input packet has TTL and TOS annotations, these
 * are copied into the encapsulating header. If those
 * annotations don't exist, the TTL and TOS header fields
 * are set to 250 and 0 respectively.
 *
 * The packet's destination, TOS, TTL, and OFF annotations
 * are set from the resulting IP encapsulation header's fields.
 *
 * The Strip element can be used by the receiver to get rid
 * of the encapsulation header.
 *
 * =e
 * Wraps packets in an IP header specifying IP protocol 4
 * (IP-in-IP), with source 18.26.4.24 and destination 140.247.60.147:
 *
 * = IPEncap(4, 18.26.4.24, 140.247.60.147)
 *
 * =a Strip
 */

#include "element.hh"
#include "glue.hh"
#include "click_ip.h"

class IPEncap : public Element {
public:
  IPEncap();
  IPEncap(int ip_p, struct in_addr ip_src, struct in_addr ip_dst);
  ~IPEncap();
  
  const char *class_name() const		{ return "IPEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  IPEncap *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
  
private:

  int _ip_p; // IP protocol number field.
  struct in_addr _ip_src;
  struct in_addr _ip_dst;

  int _id;

};

#endif
