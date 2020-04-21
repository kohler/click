// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_STRIPTCPHEADER_HH
#define CLICK_STRIPTCPHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * StripTCPHeader()
 *
 * =s tcp
 * Strip the TCP header from the front of packets
 *
 * =d
 * Removes all bytes from the beginning of the packet up to the end of the TCP
 * header.
 *
 * =e
 * Use this to get rid of all headers up to the end of the TCP layer, check if
 * the first bytes are "GET", and
 * set back the pointer to the beginning of the IP layer:
 *
 *   StripTCPHeader()
 *   -> c :: Classifier(0/474552,-)
 *   -> UnstripIPHeader()
 *
 *
 * =a Strip, StripIPHeader, UnstripTCPHeader
 */

class StripTCPHeader : public Element { public:

    StripTCPHeader() CLICK_COLD;

    const char *class_name() const		{ return "StripTCPHeader"; }
    const char *port_count() const		{ return PORTS_1_1; }

    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
