#ifndef CLICK_SETIPADDRESS_HH
#define CLICK_SETIPADDRESS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>

/*
 * =c
 * SetIPAddress(IP)
 * =s IP, annotations
 * sets destination IP address annotations
 * =d
 * Set the destination IP address annotation of incoming packets to the
 * static IP address IP.
 *
 * =a StoreIPAddress, GetIPAddress
 */

class SetIPAddress : public Element {
  
  IPAddress _ip;
  
 public:
  
  SetIPAddress();
  ~SetIPAddress();
  
  const char *class_name() const		{ return "SetIPAddress"; }
  const char *processing() const		{ return AGNOSTIC; }
  SetIPAddress *clone() const			{ return new SetIPAddress; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
