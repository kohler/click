#ifndef GETIPADDRESS_HH
#define GETIPADDRESS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>

/*
 * =c
 * GetIPAddress(OFFSET)
 * =s IP, annotations
 * sets destination IP address annotation from packet data
 * =d
 *
 * Copies 4 bytes from the packet, starting at OFFSET, to the destination IP
 * address annotation. OFFSET is usually 16, to fetch the destination address
 * from an IP packet.
 *
 * =n
 * The destination address annotation is used by elements
 * that need to know where the packet is going.
 * Such elements include ARPQuerier and LookupIPRoute.
 *
 * =a ARPQuerier, StaticIPLookup, SetIPAddress, StoreIPAddress
 */


class GetIPAddress : public Element {
  
  int _offset;
  
 public:
  
  GetIPAddress();
  ~GetIPAddress();
  
  const char *class_name() const		{ return "GetIPAddress"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  GetIPAddress *clone() const			{ return new GetIPAddress; }
  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  
};

#endif
