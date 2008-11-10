#ifndef IPENCAPPAINT_HH
#define IPENCAPPAINT_HH

/*
 * =c
 * IPEncapPaint(X, PROTOCOL, SADDR)
 * =s IP, encapsulation
 * encapsulates packets in static IP header
 * =d
 * Encapsulates each incoming packet in an IP packet with protocol
 * PROTOCOL, source address SADDR. Prepends
 * one byte for annotation X at beginning of packet.
 *
 * The final packet destination address is set to the
 * destination annotation of the packet.
 * This is most useful for IP-in-IP encapsulation.
 *
 * The packet's TTL and TOS header fields are set to 250 and 0 respectively.
 * Its destination annotation is set to DADDR.
 *
 * The IPDecapPaint element can be used by the receiver to get rid
 * of the encapsulation header, and set the Color annotation
 *
 * =e
 * Wraps packets in an IP header specifying Paint annotation 2, IP protocol 4
 * (IP-in-IP), with source 18.26.4.24:
 *
 *   IPEncapPaint(2, 4, 18.26.4.24)
 *
 * =a IPEncap, IPEncap2, UDPIPEncap, IPDecapPaint
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>

class IPEncapPaint : public Element {

  int _color; // color annotation
  int _ip_p; // IP protocol number field.
  struct in_addr _ip_src;
#ifdef __KERNEL__
  bool _aligned;
#endif

  short _id;

 public:

  IPEncapPaint();
  ~IPEncapPaint();

  const char *class_name() const		{ return "IPEncapPaint"; }
  const char *port_count() const		{ return "1/1"; }
  const char *processing() const		{ return AGNOSTIC; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  Packet *simple_action(Packet *);


};

#endif




