#ifndef ICMPPINGRESPONDER_HH
#define ICMPPINGRESPONDER_HH

/*
 * =c
 * ICMPPingResponder()
 *
 * =s ICMP
 * responds to ICMP echo requests
 *
 * =d
 *
 * Respond to ICMP echo requests. Incoming packets must have their IP header
 * annotations set. The corresponding reply is generated for any ICMP echo
 * request and emitted on output 0. IP packets other than ICMP echo requests
 * are passed along unchanged.
 *
 * =a ICMPError */

#include <click/element.hh>

class ICMPPingResponder : public Element { public:
  
  ICMPPingResponder();
  ~ICMPPingResponder();
  
  const char *class_name() const		{ return "ICMPPingResponder"; }
  const char *processing() const		{ return AGNOSTIC; }  
  ICMPPingResponder *clone() const;
  
  Packet *simple_action(Packet *);
  
};

#endif
