// -*- c-basic-offset: 4 -*-
#ifndef CLICK_LINEARIPLOOKUP_HH
#define CLICK_LINEARIPLOOKUP_HH
#include "iproutetable.hh"
CLICK_DECLS

/*
=c

LinearIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s iproute

simple IP routing table

=d

B<Note:> Lookups and table updates with LinearIPLookup are extremely slow; the
RadixIPLookup and DirectIPLookup elements should be preferred in almost all
cases.  See IPRouteTable for a performance comparison.  We provide
LinearIPLookup nevertheless for its simplicity.

Expects a destination IP address annotation with each packet. Looks up that
address in its routing table, using longest-prefix-match, sets the destination
annotation to the corresponding GW (if specified), and emits the packet on the
indicated OUTput port.

Each argument is a route, specifying a destination and mask, an optional
gateway IP address, and an output port.

LinearIPLookup uses a linear search algorithm that may look at every route on
each packet. It is therefore most suitable for small routing tables.

=e

This example delivers broadcasts and packets addressed to the local
host (18.26.4.24) to itself, packets to net 18.26.4 to the
local interface, and all others via gateway 18.26.4.1:

  ... -> GetIPAddress(16) -> rt;
  rt :: LinearIPLookup(18.26.4.24/32 0,
                       18.26.4.255/32 0,
                       18.26.4.0/32 0,
                       18.26.4/24 1,
                       0/0 18.26.4.1 1);
  rt[0] -> ToHost;
  rt[1] -> ... -> ToDevice(eth0);

=h table read-only

Outputs a human-readable version of the current routing table.

=h lookup read-only

Reports the OUTput port and GW corresponding to an address.

=h add write-only

Adds a route to the table. Format should be `C<ADDR/MASK [GW] OUT>'.
Fails if a route for C<ADDR/MASK> already exists.

=h set write-only

Sets a route, whether or not a route for the same prefix already exists.

=h remove write-only

Removes a route from the table. Format should be `C<ADDR/MASK>'.

=h ctrl write-only

Adds or removes a group of routes. Write `C<add>/C<set ADDR/MASK [GW] OUT>' to
add a route, and `C<remove ADDR/MASK>' to remove a route. You can supply
multiple commands, one per line; all commands are executed as one atomic
operation.

=a RadixIPLookup, DirectIPLookup, RangeIPLookup, StaticIPLookup,
SortedIPLookup, LinuxIPLookup, IPRouteTable */

#define IP_RT_CACHE2 1

class LinearIPLookup : public IPRouteTable { public:

    LinearIPLookup() CLICK_COLD;
    ~LinearIPLookup() CLICK_COLD;

    const char *class_name() const	{ return "LinearIPLookup"; }
    const char *port_count() const	{ return "1/-"; }
    const char *processing() const	{ return PUSH; }

    int initialize(ErrorHandler *) CLICK_COLD;

    void push(int port, Packet *p);

    int add_route(const IPRoute&, bool, IPRoute*, ErrorHandler *);
    int remove_route(const IPRoute&, IPRoute*, ErrorHandler *);
    int lookup_route(IPAddress, IPAddress&) const;
    String dump_routes();

    bool check() const;

  protected:

    Vector<IPRoute> _t;
    int _zero_route;

    IPAddress _last_addr;
    int _last_entry;

#ifdef IP_RT_CACHE2
    IPAddress _last_addr2;
    int _last_entry2;
#endif

    int lookup_entry(IPAddress) const;

};

CLICK_ENDDECLS
#endif
