// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_ADAPTIVERED_HH
#define CLICK_ADAPTIVERED_HH
#include "red.hh"
#include <click/timer.hh>

/*
=c

AdaptiveRED(MIN_THRESH, MAX_THRESH, MAX_P [, QUEUES])
AdaptiveRED(I<KEYWORDS>)

=s dropping

drops packets according to Adaptive R<RED>

=d

Implements the Adaptive Random Early Detection packet dropping algorithm. This
algorithm implements Random Early Detection, as by the RED element, plus
automatic parameter setting.

See RED for a description of AdaptiveRED's keywords and handlers.

=a RED */

class AdaptiveRED : public RED { public:

    AdaptiveRED();
    ~AdaptiveRED();

    const char *class_name() const		{ return "AdaptiveRED"; }
    AdaptiveRED *clone() const			{ return new AdaptiveRED; }
    void *cast(const char *);

    int initialize(ErrorHandler *);
    void uninitialize();

    void run_scheduled();

  protected:

    Timer _timer;

    static const int ADAPTIVE_INTERVAL = 500;
    static const uint32_t ONE_HUNDREDTH = 655;
    static const uint32_t NINE_TENTHS = 58982;
    
};

#endif
