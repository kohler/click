#ifndef CLICK_RANDOMSOURCE_HH
#define CLICK_RANDOMSOURCE_HH
#include "infinitesource.hh"
CLICK_DECLS

/*
 * =c
 * RandomSource(LENGTH [, LIMIT, BURST, ACTIVE, I<KEYWORDS>])
 * =s basicsources
 * generates random packets whenever scheduled
 * =d
 *
 * Creates packets, of the indicated length, filled with random bytes.
 * Packets' timestamp annotations are set to the current time. Pushes BURST such
 * packets out its single output every time it is scheduled (which will be
 * often). Stops sending after LIMIT packets are generated; but if LIMIT is
 * negative, sends packets forever. Will send packets only if ACTIVE is
 * true. (ACTIVE is true by default.) Default LIMIT is -1 (send packets
 * forever). Default BURST is 1.

Keyword arguments are:

=over 8

=item LENGTH

Integer. The outgoing packet will have this length.

=item LIMIT

Integer. Same as the LIMIT argument.

=item BURST

Integer. Same as the BURST argument.

=item ACTIVE

Boolean. Same as the ACTIVE argument.

=item STOP

Boolean. If true, then stop the driver once LIMIT packets are sent. Default is
false.

=item END_CALL

A write handler called once LIMIT packets are sent. END_CALL and
STOP are mutually exclusive.

=e

  RandomSource(64) -> Queue -> ...

=n

Useful for profiling and experiments.  Packets' timestamp annotations are set
to the current time.

RandomSource listens for downstream full notification.

=h count read-only
Returns the total number of packets that have been generated.
=h reset write-only
Resets the number of generated packets to 0. The RandomSource will then
generate another LIMIT packets (if it is active).
=h length read/write
Returns or sets the LENGTH parameter.
=h limit read/write
Returns or sets the LIMIT parameter.
=h burst read/write
Returns or sets the BURST parameter.
=h active read/write
Makes the element active or inactive.

=a

InfiniteSource */

class RandomSource : public InfiniteSource { public:

  RandomSource() CLICK_COLD;

  const char *class_name() const		{ return "RandomSource"; }
  void add_handlers() CLICK_COLD;

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  bool run_task(Task *);
  Packet *pull(int);

 protected:

    Packet *make_packet();

};

CLICK_ENDDECLS
#endif
