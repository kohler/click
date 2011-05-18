#ifndef CLICK_SETIPADDRESS_HH
#define CLICK_SETIPADDRESS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * SetIPAddress(IPADDR [, ANNO])
 * =s ip
 * sets destination IP address annotations
 * =d
 * Set the destination IP address annotation of incoming packets to the
 * static IP address IPADDR.  The ANNO argument, if given, sets the destination
 * IP address annotation number.
 *
 * =a StoreIPAddress, GetIPAddress
 */

class SetIPAddress : public Element {

    IPAddress _ip;
    int _anno;

 public:

  SetIPAddress();
  ~SetIPAddress();

  const char *class_name() const		{ return "SetIPAddress"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
