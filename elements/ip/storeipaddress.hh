#ifndef STOREADDRESS_HH
#define STOREADDRESS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>

/*
 * =c
 * StoreIPAddress(OFFSET)
 * =s stores dest IP address annotation in packet
 * =d
 * Copy the destination IP address annotation into the packet
 * at offset OFFSET.
 */

class StoreIPAddress : public Element {
  
  unsigned _offset;
  
 public:
  
  StoreIPAddress();
  
  const char *class_name() const		{ return "StoreIPAddress"; }
  const char *processing() const		{ return AGNOSTIC; }
  StoreIPAddress *clone() const			{ return new StoreIPAddress; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
