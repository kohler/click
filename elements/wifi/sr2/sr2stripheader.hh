#ifndef CLICK_SR2STRIPHEADER_HH
#define CLICK_SR2STRIPHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
SR2StripHeader()

=s Wifi, Wireless Routing

Strips outermost SR header

=d

Removes the outermost SR header from SR packets based on the SR Header
annotation.

=a CheckSRHeader

*/

class SR2StripHeader : public Element {

 public:
  
  SR2StripHeader();
  ~SR2StripHeader();
  
  const char *class_name() const	{ return "SR2StripHeader"; }
  const char *port_count() const	{ return PORTS_1_1; }

  Packet *simple_action(Packet *);
  
};

CLICK_ENDDECLS
#endif
