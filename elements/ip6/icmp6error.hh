#ifndef CLICK_ICMP6ERROR_HH
#define CLICK_ICMP6ERROR_HH
#include <click/element.hh>
#include <click/ip6address.hh>
CLICK_DECLS

/*
 * =c
 * ICMP6Error(IP6ADDR, TYPE, CODE)
 * =s ip6
 *
 * =d
 * need to rewrite the following comments.
 * Generate an ICMP6 error or redirect packet, with specified TYPE and CODE,
 * in response to an incoming IP6 packet. The output is an IP6/ICMP6 packet.
 * The ICMP6 packet's IP6 source address is set to IP6ADDR.
 * The error packet will include (as payload)
 * the original packet's IP6 header and the first 8 byte of the packet's
 * IP6 payload. ICMP6Error sets the packet destination IP6 and
 * fix_ip_src annotations.
 *
 * The intent is that elements that give rise to errors, like DecIP6HLIM,
 * should have two outputs, one of which is connected to an ICMP6Error.
 * Perhaps the ICMP6Error()s should be followed by a rate limiting
 * element.
 *
 * ICMP6Error never generates a packet in response to an ICMP6 error packet,
 * a fragment, or a link broadcast.
 *
 * The output of ICMPE6rror should be connected to the routing lookup
 * machinery, much as if the ICMP6 errors came from a hardware interface.
 *
 *
 *
 * =e
 * This configuration fragment produces ICMP6 Time Exceeded error
 * messages in response to HLIM expirations, but limits the
 * rate at which such messages can be sent to 10 per second:
 *
 *   dt : DecIP6HLIM();
 *   dt[1] -> ICMP6Error(18.26.4.24, 3, 0) -> m :: RatedSplitter(10) -> ...
 *   m[1] -> Discard;
 *
 * =n
 *
 * ICMP6Error can't decide if the packet's source or destination address is an
 * IP6 directed broadcast address; it is supposed to ignore packets with such
 * addresses.
 *
 * =a DecIP6HLIM */

class ICMP6Error : public Element {
public:
  ICMP6Error();
  ~ICMP6Error();

  const char *class_name() const		{ return "ICMP6Error"; }
  const char *port_count() const		{ return PORTS_1_1; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *errh) CLICK_COLD;

  Packet *simple_action(Packet *);

private:

  IP6Address _src_ip;
  int _type;
  int _code;

  static bool is_error_type(int);
  static bool is_redirect_type(int);
  bool unicast(const IP6Address &aa);
  bool valid_source(const IP6Address &aa);
  bool has_route_opt(const click_ip6 *ip);

};

CLICK_ENDDECLS
#endif
