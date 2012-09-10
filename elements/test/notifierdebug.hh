#ifndef CLICK_NOTIFIERDEBUG_HH
#define CLICK_NOTIFIERDEBUG_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c
NotifierDebug()

=s test
useful for debugging notification

=d
Responds to each packet by pushing it unchanged out its first output.  The
purpose of this element is that it provides a "signal" handler, which reports
the unparsed version of the appropriate notification signal.  If the element
is in a push context, this is the downstream full signal; if it's in a pull
context, this is the upstream empty signal.

=h signal r
Returns the unparsed version of the signal.
*/

class NotifierDebug : public Element { public:

    NotifierDebug() CLICK_COLD;

    const char *class_name() const	{ return "NotifierDebug"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int initialize(ErrorHandler *errh) CLICK_COLD;

    void add_handlers() CLICK_COLD;
    Packet *simple_action(Packet *);

  private:

    NotifierSignal _signal;

    static String read_handler(Element *, void *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
