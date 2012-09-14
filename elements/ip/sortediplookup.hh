// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SORTEDIPLOOKUP_HH
#define CLICK_SORTEDIPLOOKUP_HH
#include "lineariplookup.hh"
CLICK_DECLS

/*
=c

SortedIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s iproute

simple IP routing table

=deprecated LinearIPLookup

=d

SortedIPLookup is a version of LinearIPLookup that sorts the routing table.
In practice, however, it performs worse than LinearIPLookup, which itself
performs terribly, so it is deprecated.

=a LinearIPLookup */

class SortedIPLookup : public LinearIPLookup { public:

    SortedIPLookup() CLICK_COLD;
    ~SortedIPLookup() CLICK_COLD;

    const char *class_name() const	{ return "SortedIPLookup"; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    void push(int port, Packet *p);

    int add_route(const IPRoute&, bool, IPRoute*, ErrorHandler *);
    int remove_route(const IPRoute&, IPRoute*, ErrorHandler *);

    bool check() const;

  protected:

    inline int lookup_entry(IPAddress) const;
    void sort_table();

};

CLICK_ENDDECLS
#endif
