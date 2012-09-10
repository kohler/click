// -*- c-basic-offset: 4 -*-
#ifndef CLICK_STATICIPLOOKUP_HH
#define CLICK_STATICIPLOOKUP_HH
#include "lineariplookup.hh"
CLICK_DECLS

/*
=c

StaticIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s iproute

simple static IP routing table

=d

B<Note:> Lookups and table updates with StaticIPLookup are extremely slow; the
RadixIPLookup, DirectIPLookup, and RangeIPLookup elements should be preferred
in almost all cases.  See IPRouteTable for a performance comparison.  We
provide StaticIPLookup nevertheless for its simplicity.

This element acts like LinearIPLookup, but does not allow dynamic adding and
deleting of routes.

=h table read-only

Outputs a human-readable version of the current routing table.

=h lookup read-only

Reports the OUTput port and GW corresponding to an address.

=a RadixIPLookup, DirectIPLookup, RangeIPLookup, LinearIPLookup,
SortedIPLookup, LinuxIPLookup, IPRouteTable */

class StaticIPLookup : public LinearIPLookup { public:

    StaticIPLookup() CLICK_COLD;
    ~StaticIPLookup() CLICK_COLD;

    const char *class_name() const	{ return "StaticIPLookup"; }
    void add_handlers() CLICK_COLD;

    int add_route(const IPRoute&, bool, IPRoute*, ErrorHandler *);
    int remove_route(const IPRoute&, IPRoute*, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
