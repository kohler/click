#ifndef SETADDRESS_HH
#define SETADDRESS_HH
#include "element.hh"
#include "ipaddress.hh"

/*
 * =c
 * SetIPAddress(off)
 * =d
 * Copy the destination IP address annotation into the packet
 * at offset off.
 *
 * =a GetIPAddress
 */

class SetIPAddress : public Element {
  
  unsigned _offset;
  
 public:
  
  SetIPAddress(unsigned offset = 0);
  
  const char *class_name() const		{ return "SetIPAddress"; }
  Processing default_processing() const	{ return AGNOSTIC; }

  SetIPAddress *clone() const;
  int configure(const String &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
