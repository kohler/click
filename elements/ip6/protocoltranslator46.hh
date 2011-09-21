#ifndef CLICK_PROTOCOLTRANSLATOR46_HH
#define CLICK_PROTOCOLTRANSLATOR46_HH
#include <click/ip6address.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * ProtocolTranslator46()
 *
 *
 * =s ip6
 * translate IP/ICMP, TCP, and UDP packets between IPv4 and IPv6 protocols
 *
 * =d
 *
 *
 * Has two inputs and two outputs. Input packets are valid IPv6 packets or
 * IPv4 packets.  IPv4 packets will be translated to IPv6 packets.  IPv6
 * packets will be translated to IPv4 packets.  Output packets are valid
 * IPv4/v6 packets; for instance, translated packets have their IP, ICMP/ICMPv6,
 * TCP and/or UDP checksums updated.
 *
 *
 * =a AddressTranslator ProtocolTranslator64*/

class ProtocolTranslator46 : public Element {


 public:

  ProtocolTranslator46();
  ~ProtocolTranslator46();

  const char *class_name() const		{ return "ProtocolTranslator46"; }
  const char *port_count() const		{ return PORTS_1_1; }
  void push(int port, Packet *p);
  void handle_ip4(Packet *);

private:

  Packet * make_icmp_translate46(IP6Address ip6_src,
				 IP6Address ip6_dst,
				 unsigned char *a,
				 unsigned char payload_length);

  Packet * make_translate46(IP6Address src,
			    IP6Address dst,
			    click_ip * ip,
			    unsigned char *a);

};

CLICK_ENDDECLS
#endif
