#ifndef CLICK_BURSTER_HH
#define CLICK_BURSTER_HH
#include "timedunqueue.hh"
CLICK_DECLS

/*
 * =c
 * Burster(INTERVAL [, BURST])
 * =s shaping
 * pull-to-push converter
 * =d
 *
 * Burster is a variant of TimedUnqueue with a default BURST of 8, rather than
 * 1.
 *
 * =a TimedUnqueue
 */

class Burster : public TimedUnqueue { public:

    Burster();

    const char *class_name() const		{ return "Burster"; }
    void *cast(const char *name);

};

CLICK_ENDDECLS
#endif
