#ifndef CLICK_RATEDSOURCE_HH
#define CLICK_RATEDSOURCE_HH
#include <click/element.hh>
#include <click/gaprate.hh>
#include <click/task.hh>

/*
=c

RatedSource([DATA, RATE, LIMIT, ACTIVE, I<KEYWORDS>])

=s sources

generates packets at specified rate

=d

Creates packets consisting of DATA. Pushes at most LIMIT such packets out
its single output at a rate of RATE packets per second. It will send a
maximum of one packet per scheduling, so very high RATEs may not be
achievable. If LIMIT is negative, sends packets forever. Will send packets
only if ACTIVE is true. Default DATA is at least 64 bytes long. Default
RATE is 10. Default LIMIT is -1 (send packets forever). Default ACTIVE is
true.

Keyword arguments are:

=over 8

=item DATA

String. Same as the DATA argument.

=item RATE

Integer. Same as the RATE argument.

=item LIMIT

Integer. Same as the LIMIT argument.

=item ACTIVE

Boolean. Same as the ACTIVE? argument.

=item STOP

Boolean. If true, then stop the driver once LIMIT packets are sent. Default is
false.

=back

To generate a particular repeatable traffic pattern, use this element's
B<rate> and B<active> handlers in conjunction with PokeHandlers.

=e

  RatedSource(\<0800>, 10, 1000) -> Queue -> ...

=h count read-only
Returns the total number of packets that have been generated.
=h reset write-only
Resets the number of generated packets to 0. The RatedSource will then
generate another LIMIT packets (if it is active).
=h rate read/write
Returns or sets the RATE parameter.
=h limit read/write
Returns or sets the LIMIT parameter. Negative numbers mean no limit.
=h active read/write
Makes the element active or inactive.

=a

InfiniteSource, PokeHandlers */

class RatedSource : public Element { public:

  RatedSource();
  ~RatedSource();
  
  const char *class_name() const		{ return "RatedSource"; }
  const char *processing() const		{ return AGNOSTIC; }
  void add_handlers();
  
  RatedSource *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  Packet *pull(int);
  
 protected:
  
  static const unsigned NO_LIMIT = 0xFFFFFFFFU;

  GapRate _rate;
  unsigned _count;
  unsigned _limit;
  bool _active : 1;
  bool _stop : 1;
  Packet *_packet;
  Task _task;
  String _data;

  static String read_param(Element *, void *);
  static int change_param(const String &, Element *, void *, ErrorHandler *);
  
};

#endif
