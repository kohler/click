// -*- c-basic-offset: 4 -*-
#ifndef CLICK_LINEARIPLOOKUP_HH
#define CLICK_LINEARIPLOOKUP_HH

/*
=c

LinearIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s IP, classification

simple IP routing table

=d

Expects a destination IP address annotation with each packet. Looks up that
address in its routing table, using longest-prefix-match, sets the destination
annotation to the corresponding GW (if specified), and emits the packet on the
indicated OUTput port.

Each argument is a route, specifying a destination and mask, an optional
gateway IP address, and an output port.

LinearIPLookup uses a linear search algorithm that may look at every route on
each packet. It is therefore most suitable for small routing tables.

Two or more route arguments may refer to the same address prefix. The
last-specified route takes precedence. However, the earlier-specified routes
remain in the table; if the last-specified route is deleted, an
earlier-specified route will take over.

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

=h add write-only

Adds a route to the table. Format should be `C<ADDR/MASK [GW] OUT>'.

=h remove write-only

Removes a route from the table. Format should be `C<ADDR/MASK [GW] OUT>', to
remove a specific route, or `C<ADDR/MASK>', to remove all routes for a given
prefix (that is, all routes with the same ADDR and MASK).

=h ctrl write-only

Adds or removes routes. Write `C<add ADDR/MASK [GW] OUT>' to add a route, and
`C<remove ADDR/MASK [[GW] OUT]>' to remove a route.

=a StaticIPLookup, RadixIPLookup */

#include "iproutetable.hh"

#define IP_RT_CACHE2 1

class LinearIPLookup : public IPRouteTable { public:

    LinearIPLookup();
    ~LinearIPLookup();

    const char *class_name() const	{ return "LinearIPLookup"; }
    const char *processing() const	{ return PUSH; }
    LinearIPLookup *clone() const	{ return new LinearIPLookup; }

    void notify_noutputs(int);
    int initialize(ErrorHandler *);
    void add_handlers();

    void push(int port, Packet *p);

    int add_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);
    int remove_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);
    int lookup_route(IPAddress, IPAddress &) const;
    String dump_routes() const;

    bool check() const;

    struct Entry {
	IPAddress addr, mask, gw;
	int output, next;
	Entry(IPAddress d, IPAddress m, IPAddress g, int o) : addr(d), mask(m), gw(g), output(o), next(0x7FFFFFFF) { }
	String unparse_addr() const { return addr.unparse_with_mask(mask); }
    };

  private:

    Vector<Entry> _t;

    IPAddress _last_addr;
    int _last_entry;

#ifdef IP_RT_CACHE2
    IPAddress _last_addr2;
    int _last_entry2;
#endif

    Vector<Entry> _redundant;

    int lookup_entry(IPAddress) const;

};

#endif
