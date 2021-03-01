// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TOHOST_USERLEVEL_HH
#define CLICK_TOHOST_USERLEVEL_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include "fromhost.hh"
CLICK_DECLS

/*
 * =title ToHost.u
 *
 * =c
 *
 * ToHost(DEVNAME)
 *
 * =s comm
 *
 * sends packets to Linux via Universal TUN/TAP device.
 *
 * =d
 *
 * Hands packets to the ordinary Linux protocol stack.
 * Expects packets with Ethernet headers.
 *
 * You should probably give Linux IP packets addressed to
 * the local machine (including broadcasts), and a copy
 * of each ARP reply.
 *
 * ToHost requires an initialized FromHost with the same DEVNAME.
 *
 * IPv4 packets should have a destination IP address corresponding
 * to DEVNAME, and a routable source address. Otherwise Linux will silently
 * drop the packets.
 *
 * =h drops read-only
 *
 * Reports the number of packets ToHost has dropped because they had a null
 * device annotation.
 *
 * =a
 *
 * FromHost.u, FromHost
 *
 */

class ToHost : public Element {
public:
    ToHost() CLICK_COLD;
    ~ToHost() CLICK_COLD;

    const char *class_name() const	{ return "ToHost"; }
    const char *port_count() const	{ return PORTS_1_0; }
    const char *processing() const	{ return PUSH; }

    int configure_phase() const		{ return FromHost::CONFIGURE_PHASE_TOHOST; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    ToHost *hotswap_element() const;
    void take_state(Element *, ErrorHandler *);

    void push(int port, Packet *);

  private:
    int _fd;
    int _drops;
    String _dev_name;

    int find_fromhost(ErrorHandler *);

};

CLICK_ENDDECLS
#endif
