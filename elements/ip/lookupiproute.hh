// -*- c-basic-offset: 4 -*-
#ifndef CLICK_STATICIPLOOKUP_HH
#define CLICK_STATICIPLOOKUP_HH

/*
=c

StaticIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s IP, classification

simple static IP routing table

=d

This element acts like LinearIPLookup, but does not allow dynamic adding and
deleting of routes.

=h table read-only

Outputs a human-readable version of the current routing table.

=a LinearIPLookup, LinuxIPLookup, RadixIPLookup */

#include "lineariplookup.hh"

class StaticIPLookup : public LinearIPLookup { public:

    StaticIPLookup();
    ~StaticIPLookup();

    const char *class_name() const	{ return "StaticIPLookup"; }
    StaticIPLookup *clone() const	{ return new StaticIPLookup; }

    void add_handlers();
    
    int add_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);
    int remove_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);

};

#endif
