#ifndef STOREADDRESS_HH
#define STOREADDRESS_HH
#include "element.hh"
#include "ipaddress.hh"

/*
 * =c
 * StoreIPAddress(OFFSET)
 * =d
 * Copy the destination IP address annotation into the packet
 * at offset OFFSET.
 *
 * =a LoadIPAddress
 */

class StoreIPAddress : public Element {
  
  unsigned _offset;
  
 public:
  
  StoreIPAddress();
  
  const char *class_name() const		{ return "StoreIPAddress"; }
  const char *processing() const		{ return AGNOSTIC; }
  StoreIPAddress *clone() const			{ return new StoreIPAddress; }
  
  int configure(const String &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
