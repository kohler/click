#ifndef SetRandIPAddress_hh
#define SetRandIPAddress_hh
#include <click/element.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * SetRandIPAddress(PREFIX, [LIMIT])
 * =s ip
 * sets destination IP address annotations randomly
 * =d
 * Set the destination IP address annotation to a random number within
 * the specified PREFIX.
 *
 * If LIMIT is given, at most LIMIT distinct addresses will be generated.
 *
 * =a StoreIPAddress, GetIPAddress, SetIPAddress
 */

class SetRandIPAddress : public Element {

  IPAddress _ip;
  IPAddress _mask;
  int _max;
  IPAddress *_addrs;

 public:

  SetRandIPAddress() CLICK_COLD;
  ~SetRandIPAddress() CLICK_COLD;

  const char *class_name() const	{ return "SetRandIPAddress"; }
  const char *port_count() const	{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);
  IPAddress pick();
};

CLICK_ENDDECLS
#endif
