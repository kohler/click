#ifndef GETIPADDRESS_HH
#define GETIPADDRESS_HH
#include "element.hh"
#include "ipaddress.hh"

/*
 * =c
 * GetIPAddress(offset)
 * =d
 * Copies 4 bytes from the packet to the destination IP address annotation.
 * The offset is usually 16, to fetch the dst address from
 * an IP packet (w/o ether header).
 *
 * The destination address annotation is used by elements
 * that need to know where the packet is going.
 * Such elements include ArpQuerier and SetIPRoute.
 *
 * =a ArpQuerier
 * =a LinuxSetIPRoute
 * =a SetIPRoute
 */


class GetIPAddress : public Element {
  
  int _offset;
  
 public:
  
  GetIPAddress(int = 0);
  
  const char *class_name() const		{ return "GetIPAddress"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  GetIPAddress *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  
  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

#endif
