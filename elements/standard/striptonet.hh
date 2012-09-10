// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_STRIPTONET_HH
#define CLICK_STRIPTONET_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 *
 * StripToNetworkHeader()
 *
 * =s basicmod
 *
 * strips everything preceding network header
 *
 * =d
 *
 * Strips any data preceding the network header from every passing packet.
 * Requires a network header annotation, such as an IP header annotation,
 * on every packet.
 * If the packet's network header annotation points before the start of the
 * packet data, then StripToNetworkHeader will move the packet data pointer
 * back, to point at the network header.
 *
 * =a Strip
 */

class StripToNetworkHeader : public Element { public:

    StripToNetworkHeader() CLICK_COLD;

    const char *class_name() const	{ return "StripToNetworkHeader"; }
    const char *port_count() const	{ return PORTS_1_1; }

    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
