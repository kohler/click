// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_ADAPTIVERED_HH
#define CLICK_ADAPTIVERED_HH
#include "red.hh"
#include <click/timer.hh>
CLICK_DECLS

/*
=c

AdaptiveRED(TARGET, MAX_P [, I<KEYWORDS>])

=s aqm

drops packets according to Adaptive P<RED>

=d

Implements the Adaptive Random Early Detection packet dropping algorithm. This
algorithm implements Random Early Detection, as by the RED element, plus
automatic parameter setting.

The TARGET argument is the target queue length. RED's MIN_THRESH parameter
is set to TARGET/2, and MAX_THRESH to 3*TARGET/2. The MAX_P parameter, and
QUEUES and STABILITY keywords, are as in the RED element.

=a RED */

class AdaptiveRED : public RED { public:

    AdaptiveRED() CLICK_COLD;
    ~AdaptiveRED() CLICK_COLD;

    const char *class_name() const		{ return "AdaptiveRED"; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;

    void run_timer(Timer *);

  protected:

    Timer _timer;

    static const int ADAPTIVE_INTERVAL = 500;
    static const uint32_t ONE_HUNDREDTH = 655;
    static const uint32_t NINE_TENTHS = 58982;

};

CLICK_ENDDECLS
#endif
