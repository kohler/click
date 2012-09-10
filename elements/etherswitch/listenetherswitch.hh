#ifndef CLICK_LISTENETHERSWITCH_HH
#define CLICK_LISTENETHERSWITCH_HH
#include "etherswitch.hh"
CLICK_DECLS

/*
=c

ListenEtherSwitch([I<keywords> TIMEOUT])

=s ethernet

learning, forwarding Ethernet switch with listen port

=d

A version of EtherSwitch with a listen port.  Where EtherSwitch has exactly as
many inputs as it has outputs, ListenEtherSwitch has one more output than it
has inputs.  This last output is the listen port; a copy of every received
packet is sent to that port.  In an element with N inputs and N+1 outputs, the
(N+1)th output is the listen port.  See EtherSwitch for more information.

=a

ListenEtherSwitch
*/

class ListenEtherSwitch : public EtherSwitch { public:

    ListenEtherSwitch() CLICK_COLD;
    ~ListenEtherSwitch() CLICK_COLD;

    const char *class_name() const		{ return "ListenEtherSwitch"; }
    const char *port_count() const		{ return "-/=+"; }

    void push(int port, Packet* p);

};

CLICK_ENDDECLS
#endif
