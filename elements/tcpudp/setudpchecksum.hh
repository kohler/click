// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SETUDPCHECKSUM_HH
#define CLICK_SETUDPCHECKSUM_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * SetUDPChecksum()
 * =s udp
 * sets UDP packets' checksums
 * =d
 * Input packets must be UDP in IP (the protocol field isn't checked).
 *
 * Calculates the UDP checksum and sets the UDP header's checksum field. Uses
 * IP header fields to generate the pseudo-header.
 *
 * If input packets are IP fragments, or the UDP length is longer than the
 * packet, then pushes the input packets to the 2nd output, or drops them with
 * a warning if there is no 2nd output.
 *
 * =a CheckUDPHeader, SetIPChecksum, CheckIPHeader, SetTCPChecksum */

class SetUDPChecksum : public Element { public:

    SetUDPChecksum() CLICK_COLD;
    ~SetUDPChecksum() CLICK_COLD;

    const char *class_name() const	{ return "SetUDPChecksum"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }

    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
