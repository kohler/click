// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SETUDPCHECKSUM_HH
#define CLICK_SETUDPCHECKSUM_HH

/*
 * =c
 * SetUDPChecksum()
 * =s UDP
 * sets UDP packets' checksums
 * =d
 * Input packets should be UDP in IP.
 *
 * Calculates the UDP checksum and sets the UDP header's checksum field. Uses
 * IP header fields to generate the pseudo-header.
 *
 * =a CheckUDPHeader, SetIPChecksum, CheckIPHeader, SetTCPChecksum */

#include <click/element.hh>
#include <click/glue.hh>

class SetUDPChecksum : public Element { public:
    
    SetUDPChecksum();
    ~SetUDPChecksum();
  
    const char *class_name() const	{ return "SetUDPChecksum"; }
    const char *processing() const	{ return AGNOSTIC; }
    SetUDPChecksum *clone() const	{ return new SetUDPChecksum; }

    Packet *simple_action(Packet *);

};

#endif
