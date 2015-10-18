#ifndef CLICK_ETHERREWRITE_HH
#define CLICK_ETHERREWRITE_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
=c

EtherRewrite(SRC, DST)

=s ethernet

rewrite source and destination Ethernet address

=d

Rewrite the source and the destination address in the Ethernet header.
The ETHERTYPE is left untouched.

=e

Ensure that source address of all packets passing by is 1:1:1:1:1:1, and
the destination address is 2:2:2:2:2:2:

  EtherRewrite(1:1:1:1:1:1, 2:2:2:2:2:2)

=n

This element is useful if there is only one possible nexthop on a given link
(such as for a network-layer transparent firewall), meaning that source and
destination ethernet address will always be the same for a given output.

=h src read/write

Return or set the SRC parameter.

=h dst read/write

Return or set the DST parameter.

=a

EtherEncap, EtherVLANEncap, ARPQuerier, EnsureEther, StoreEtherAddress */


class EtherRewrite : public Element { public:

    EtherRewrite() CLICK_COLD;
    ~EtherRewrite() CLICK_COLD;

    const char *class_name() const	{ return "EtherRewrite"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    void add_handlers() CLICK_COLD;

    inline Packet *smaction(Packet *);

    Packet *pull(int);

    void push(int, Packet *);

  private:

    click_ether _ethh;

};

CLICK_ENDDECLS
#endif
