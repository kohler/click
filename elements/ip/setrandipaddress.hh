#ifndef SetRandIPAddress_hh
#define SetRandIPAddress_hh
#include <click/element.hh>
#include <click/ipaddress.hh>

/*
 * =c
 * SetRandIPAddress(ADDR/LEN, [MAX])
 * =s IP, annotations
 * sets destination IP address annotations randomly
 * =d
 * Set the destination IP address annotation to a random number whose
 * upper LEN bits are equal to ADDR.
 *
 * If MAX is given, at most MAX distinct addresses will be generated.
 *
 * =a StoreIPAddress, GetIPAddress, SetIPAddress
 */

class SetRandIPAddress : public Element {
  
  IPAddress _ip;
  IPAddress _mask;
  int _max;
  IPAddress *_addrs;
  
 public:
  
  SetRandIPAddress();
  ~SetRandIPAddress();
  
  const char *class_name() const	{ return "SetRandIPAddress"; }
  const char *processing() const	{ return AGNOSTIC; }
  SetRandIPAddress *clone() const	{ return new SetRandIPAddress; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  IPAddress pick();
};

#endif
