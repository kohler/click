// -*- c-basic-offset: 4 -*-
#ifndef CLICK_QUEUEYANKTEST_HH
#define CLICK_QUEUEYANKTEST_HH
#include <click/element.hh>
#include <click/timer.hh>
#include "elements/standard/simplequeue.hh"
CLICK_DECLS

/*
=c

QueueYankTest(QUEUE)

=s test

check packets against a specification

=d

QueueYankTest compares all received packets against a specification provided by
keyword arguments. It prints error messages when incoming packets don't match
the spec.

Keyword arguments are as follows. Tests are performed for the keyword
arguments you specify. If you don't want to run a test, don't supply the
keyword. QueueYankTest(), with no keywords, accepts every packet.

=over 8

=item DATA

String. The contents of the packet (starting DATA_OFFSET bytes in) must
exactly match DATA.

=item DATA_OFFSET

Integer. Specifies the offset into the packet used for DATA matches. Default
is 0.

=item LENGTH

Integer. The packet's length must equal LENGTH.

=item LENGTH_GE

Integer. The packet's length must be at least LENGTH_GE.

=item LENGTH_LE

Integer. The packet's length must be at most LENGTH_LE. Specify at most one of
LENGTH, LENGTH_GE, and LENGTH_LE.

=item ALIGNMENT

Two space-separated integers, `MODULUS OFFSET'. The packet's data must be
aligned OFFSET bytes off from a MODULUS-byte boundary.

=a

PacketTest */

class QueueYankTest : public Element { public:

    QueueYankTest() CLICK_COLD;

    const char *class_name() const		{ return "QueueYankTest"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;

    void run_timer(Timer *);

  private:

    SimpleQueue *_q;
    Timer _t;

};

CLICK_ENDDECLS
#endif
