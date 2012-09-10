// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RANGEIPLOOKUP_HH
#define CLICK_RANGEIPLOOKUP_HH
#include "iproutetable.hh"
#include "directiplookup.hh"
CLICK_DECLS

/*
=c

RangeIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s iproute

IP routing lookup through binary search in a very compact table

=d

Expects a destination IP address annotation with each packet. Looks up that
address in its routing table, using longest-prefix-match, sets the destination
annotation to the corresponding GW (if specified), and emits the packet on the
indicated OUTput port.

Each argument is a route, specifying a destination and mask, an optional
gateway IP address, and an output port.  No destination-mask pair should occur
more than once.

RangeIPLookup aims at achieving high lookup speeds through exploiting the CPU
cache locality.  The routing table is expanded into a very small lookup
structure, typically occupying less then 4 bytes per IP prefix.  As an example,
a lookup structure corresponding to a routing table with 167000 entries (a
realistic snapshot taken from a core Internet router) occupies only around
512 KBytes of RAM.  Depending on how sucessfully the CPU cache
affinity can be maintained, worst-case lookup rates exceeding 20 million
lookups per second can be achieved using modern commodity CPUs.

RangeIPLookup maintains a large DirectIPLookup table as well as its own
tables.  Although this subsidiary table is only accessed during route updates,
it significantly adds to RangeIPLookup's total memory footprint.

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

=h flush write-only

Clears the entire routing table in a single atomic operation.

=n

See IPRouteTable for a performance comparison of the various IP routing
elements.

=a IPRouteTable, RadixIPLookup, DirectIPLookup, LinearIPLookup,
SortedIPLookup, StaticIPLookup, LinuxIPLookup

*/

class RangeIPLookup : public IPRouteTable { public:

    RangeIPLookup() CLICK_COLD;
    ~RangeIPLookup() CLICK_COLD;

    const char *class_name() const      { return "RangeIPLookup"; }
    const char *port_count() const	{ return "1/-"; }
    const char *processing() const      { return PUSH; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    void push(int port, Packet* p);

    int add_route(const IPRoute&, bool, IPRoute*, ErrorHandler *);
    int remove_route(const IPRoute&, IPRoute*, ErrorHandler *);
    int lookup_route(IPAddress, IPAddress&) const;
    String dump_routes();

    static int flush_handler(const String &, Element *, void *, ErrorHandler *);

  protected:

    void flush_table();
    void expand();

    enum { KICKSTART_BITS = 12 };
    enum { RANGES_MAX = 256 * 1024 };
    enum { RANGE_MASK = 0xffffffff >> KICKSTART_BITS };
    enum { RANGE_SHIFT = 32 - KICKSTART_BITS };

    uint32_t *_range_base;
    uint32_t *_range_len;
    uint32_t *_range_t;
    bool _active;

    DirectIPLookup::Table _helper;

};

CLICK_ENDDECLS
#endif
