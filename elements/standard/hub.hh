#ifndef CLICK_HUB_HH
#define CLICK_HUB_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Hub()

=s basictransfer

duplicates packets like a hub

=d

Hub sends a copy of each input packet out each output port.  However, a packet
received on input port N is not emitted on output port N.  Thus, the element
acts sort of like an Ethernet hub (but it is not Ethernet specific).

=a

Tee, EtherSwitch
*/

class Hub : public Element { public:

    Hub() CLICK_COLD;

    const char *class_name() const		{ return "Hub"; }
    const char *port_count() const		{ return "-/="; }
    const char *processing() const		{ return PUSH; }
    const char *flow_code() const		{ return "#/[^#]"; }

    void push(int port, Packet* p);

};

CLICK_ENDDECLS
#endif
