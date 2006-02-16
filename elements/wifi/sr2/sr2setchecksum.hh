#ifndef SR2SETCHECKSUM_HH
#define SR2SETCHECKSUM_HH

/*
=c

SR2SetChecksum()

=s Wifi, Wireless Routing

Set Checksum for Source Routed packet.

=d

Expects a SR MAC packet as input. Calculates the SR header's checksum
and sets the version and checksum header fields.

=a CheckSRHeader 
*/

#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

class SR2SetChecksum : public Element {
public:
  SR2SetChecksum();
  ~SR2SetChecksum();
  
  const char *class_name() const		{ return "SR2SetChecksum"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
