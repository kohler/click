#ifndef SETIP6ADDRESS_HH
#define SETIP6ADDRESS_HH
#include "element.hh"
#include "ip6address.hh"

/*
 * =c
 * SetIP6Address(IP6)
 * =d
 * Set the destination IP6 address annotation of incoming packets to the
 * static IP6 address IP6
 *
 * =a StoreIP6Address, GetIP6Address
 */

class SetIP6Address : public Element {

  IP6Address _ip6;
  
 public:
  
  SetIP6Address();
  
  const char *class_name() const		{ return "SetIP6Address"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetIP6Address *clone() const                  { return new SetIP6Address; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
