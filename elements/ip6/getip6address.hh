#ifndef GETIP6ADDRESS_HH
#define GETIP6ADDRESS_HH
#include "element.hh"
#include "ip6address.hh"

/*
 * =c
 * GetIP6Address(offset)
 * =d
 * Copies 16 bytes from the packet , starting at OFFSET, to the destination IP6 
 * address annotation.  The offset is usually 16, to fetch the dst address from
 * an IP6 packet (w/o ether header).
 *
 * The destination address annotation is used by elements
 * that need to know where the packet is going.
 * Such elements include ArpQuerier6 and LookupIP6Route.
 *
 * =a ArpQuerier
 * =a LookupIP6Route
 * =a SetIP6Address StoreIP6Address
 */


class GetIP6Address : public Element {
  
  int _offset;
  
 public:
  
  GetIP6Address();
  
  const char *class_name() const		{ return "GetIP6Address"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  GetIP6Address *clone() const { return new GetIP6Address; }
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  //inline void smaction(Packet *);
  //void push(int, Packet *p);
  //Packet *pull(int);
  
};

#endif
