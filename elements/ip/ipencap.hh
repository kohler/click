#ifndef IPENCAP_HH
#define IPENCAP_HH

/*
=c

IPEncap(PROTOCOL, SRC, DST)

=s IP, encapsulation

encapsulates packets in static IP header

=d

Encapsulates each incoming packet in an IP packet with protocol
PROTOCOL, source address SRC, and destination address DST.
This is most useful for IP-in-IP encapsulation.

The packet's TTL and TOS header fields are set to 250 and 0 respectively.
Its destination annotation is set to DADDR.

The StripIPHeader element can be used by the receiver to get rid
of the encapsulation header.

=e

Wraps packets in an IP header specifying IP protocol 4
(IP-in-IP), with source 18.26.4.24 and destination 140.247.60.147:

  IPEncap(4, 18.26.4.24, 140.247.60.147)

=h src read/write

Returns or sets the SRC parameter.

=h dst read/write

Returns or sets the DST parameter.

=a UDPIPEncap, StripIPHeader
*/

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/click_ip.h>

class IPEncap : public Element { public:
  
  IPEncap();
  ~IPEncap();
  
  const char *class_name() const		{ return "IPEncap"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  IPEncap *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  int initialize(ErrorHandler *);
  void add_handlers();

  Packet *simple_action(Packet *);
  
 private:
  
  int _ip_p; // IP protocol number field.
  struct in_addr _ip_src;
  struct in_addr _ip_dst;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  bool _aligned;
#endif

  u_atomic32_t _id;

  static String read_handler(Element *, void *);
  
};

#endif
