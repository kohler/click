// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SORTEDIPLOOKUP_HH
#define CLICK_SORTEDIPLOOKUP_HH
#include "lineariplookup.hh"
CLICK_DECLS

/*
=c

SortedIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s IP, classification

simple IP routing table

=d

SortedIPLookup is a version of LinearIPLookup that sorts the routing table.
This may, or may not, marginally speed up its operation relative to
LinearIPLookup. Its worst-case lookup time is still O(N), where N is the
number of routes.

=a LinearIPLookup */

class SortedIPLookup : public LinearIPLookup { public:

    SortedIPLookup();
    ~SortedIPLookup();

    const char *class_name() const	{ return "SortedIPLookup"; }
    SortedIPLookup *clone() const	{ return new SortedIPLookup; }

    void push(int port, Packet *p);

    int add_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);
    int remove_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);

    bool check() const;

  protected:

    inline int lookup_entry(IPAddress) const;
    void sort_table();

};

CLICK_ENDDECLS
#endif
