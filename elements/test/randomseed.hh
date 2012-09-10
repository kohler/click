// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RANDOMSEED_HH
#define CLICK_RANDOMSEED_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

RandomSeed([SEED])

=s control

sets random seed

=d

RandomSeed sets the random seed to the SEED argument.  If not supplied, the
random seed is set to a "truly random" value.  (This is not generally useful
since Click resets the random seed to a "truly random" value whenever a router
is configured.)

=h seed write-only

Write this handler to reset the random seed, either to a particular value or
(if you supply an empty argument) to a "truly random" value.

*/

class RandomSeed : public Element { public:

    RandomSeed() CLICK_COLD;

    const char *class_name() const	{ return "RandomSeed"; }

    int configure_phase() const		{ return CONFIGURE_PHASE_FIRST; }
    bool can_live_reconfigure() const	{ return true; }
    int configure(Vector<String>&, ErrorHandler*) CLICK_COLD;
    void add_handlers() CLICK_COLD;

};

CLICK_ENDDECLS
#endif
