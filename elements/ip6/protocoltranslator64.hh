#ifndef PROTOCOLTRANSLATOR64_HH
#define PROTOCOLTRANSLATOR64_HH

#include <click/ip6address.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/element.hh>

/*
 * =c
 * ProtocolTranslator64()
 *
 *
 * =s IPv6
 * translate IP/ICMP, TCP, and UDP packets from the IPv6 to the IPv4 protocol
 *
 * =d
 *
 * 
 * Has one input and one output. Input packets are valid IPv6 packets.  IPv6 
 * packets will be translated to IPv4 packets.  Output packets are valid 
 * IPv4 packets; for instance, translated packets have their IP, ICMP, 
 * TCP and/or UDP checksums updated.
 *
 * 
 * =a AddressTranslator ProtocolTranslator46*/

class ProtocolTranslator64 : public Element {
  
  
 public:
  
  ProtocolTranslator64();
  ~ProtocolTranslator64();
  
  const char *class_name() const		{ return "ProtocolTranslator64"; }
  const char *processing() const	{ return AGNOSTIC; }
  ProtocolTranslator64 *clone() const { return new ProtocolTranslator64; }
  int configure(Vector<String> &, ErrorHandler *);
  void push(int port, Packet *p);
  void handle_ip6(Packet *);
 
private:

  Packet * make_icmp_translate64(unsigned char *a,
				unsigned char payload_length);
  
  Packet * make_translate64(IPAddress src,
			    IPAddress dst,
			    click_ip6 * ip6,
			    unsigned char *a); 



};
		   

#endif
