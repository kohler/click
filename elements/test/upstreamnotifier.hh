#ifndef CLICK_UPSTREAMNOTIFIER_HH
#define CLICK_UPSTREAMNOTIFIER_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c
UpstreamNotifier([SIGNAL])

=s test
null element with an upstream notifier

=d
Responds to each packet by pushing it unchanged out its first output.
Also has an upstream notifier, and is generally used
for testing upstream notification.

Keyword arguments are:

=over 8

=item SIGNAL
Boolean. Whether to set the upstream signal to active.

=h signal read/write
Returns if the signal is active. Same as the SIGNAL argument.


=a
PullNull, Script, InfiniteSource
*/

class UpstreamNotifier : public Element { public:

  UpstreamNotifier();

  const char *class_name() const	{ return "UpstreamNotifier"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }

  void *cast(const char *);
  int configure(Vector<String> &conf, ErrorHandler *);

  void add_handlers();
  void push(int, Packet *);

  ActiveNotifier _notifier;
};

CLICK_ENDDECLS
#endif
