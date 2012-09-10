// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ERASEIPPAYLOAD_HH
#define CLICK_ERASEIPPAYLOAD_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/ip.h>
CLICK_DECLS

/*
=c

EraseIPPayload()

=s ip

erases IP packet payload

=d

Erases all TCP or UDP payload in incoming packets.  Output packets
have the same length, but all payload bytes are zero.

=a AnonymizeIPAddr */

class EraseIPPayload : public Element { public:

    EraseIPPayload() CLICK_COLD;
    ~EraseIPPayload() CLICK_COLD;

    const char *class_name() const	{ return "EraseIPPayload"; }
    const char *port_count() const	{ return PORTS_1_1; }

    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
