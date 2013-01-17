#ifndef CLICK_SETIPADDRESS_HH
#define CLICK_SETIPADDRESS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * SetIPAddress(ADDR [, ANNO])
 * =s ip
 * sets destination IP address annotations
 * =d
 * Set the destination IP address annotation of incoming packets to the
 * static IP address ADDR. The ANNO argument, if given, sets the destination
 * IP address annotation number.
 *
 * =h addr rw
 * Returns or sets the ADDR argument.
 *
 * =a StoreIPAddress, GetIPAddress
 */

class SetIPAddress : public Element {

    IPAddress _ip;
    int _anno;

  public:

    SetIPAddress() CLICK_COLD;
    ~SetIPAddress() CLICK_COLD;

    const char *class_name() const		{ return "SetIPAddress"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
