#ifndef CLICK_STRIPSRHEADER_HH
#define CLICK_STRIPSRHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
StripSRHeader()

=s Wifi, Wireless Routing

Strips outermost SR header

=d

Removes the outermost SR header from SR packets based on the SR Header
annotation.

=a CheckSRHeader

*/

class StripSRHeader : public Element {

 public:
  
  StripSRHeader();
  ~StripSRHeader();
  
  const char *class_name() const		{ return "StripSRHeader"; }

  Packet *simple_action(Packet *);
  
};

CLICK_ENDDECLS
#endif
