#ifndef ICMPRESPONDER_HH
#define ICMPRESPONDER_HH

/*
 * =c
 * ICMPPing()
 * =d
 * Respond to ICMP pings. The input packet must be an ethernet packet with
 * link header. Respond by modifying that same ether packet.
 *
 * =a ICMPError
 */

#include "element.hh"

class ICMPPing : public Element {
  
  Packet *make_echo_response(Packet *);

 public:
  
  ICMPPing();
  
  const char *class_name() const		{ return "ICMPPing"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  ICMPPing *clone() const;
  Packet *simple_action(Packet *);
  
};

#endif
