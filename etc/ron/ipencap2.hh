#ifndef IPENCAP2_HH
#define IPENCAP2_HH

/*
 * =c
 * IPEncap2(PROTOCOL, SADDR)
 * =s IP, encapsulation
 * encapsulates packets in static IP header
 * =d
 * Encapsulates each incoming packet in an IP packet with protocol
 * PROTOCOL, source address SADDR, and destination address set to the
 * destination annotation of the packet.
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
 * (IP-in-IP), with source 18.26.4.24:
 *
 *   IPEncap2(4, 18.26.4.24)
 *
 * =a IPEncap, UDPIPEncap, Strip
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>

class IPEncap2 : public Element {

  int _ip_p; // IP protocol number field.
  struct in_addr _ip_src;
#ifdef __KERNEL__
  bool _aligned;
#endif

  short _id;

 public:

  IPEncap2();
  ~IPEncap2();

  const char *class_name() const		{ return "IPEncap2"; }
  const char *port_count() const		{ return "1/1"; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  Packet *simple_action(Packet *);

};

#endif
