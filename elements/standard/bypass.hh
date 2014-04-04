#ifndef CLICK_BYPASS_HH
#define CLICK_BYPASS_HH 1
#include <click/element.hh>
#include <click/routervisitor.hh>
CLICK_DECLS

/*
=c

Bypass([ACTIVE])

=s classification

sends packet stream through optional bypass with zero overhead

=d

Bypass adds zero-overhead path switching to a configuration.

Normally, a Bypass element emits received packets on its first output, like
Null. However, when ACTIVE is true, a push Bypass element emits packets onto
output port 1. An active pull Bypass forwards pull requests to input port 1.

Here's a typical use of a push Bypass:

  ... source -> print_bypass :: Bypass -> sink ...;
  print_bypass [1] -> Print(Debug) -> sink;

To turn on the Print, write print_bypass.active to true.

You could implement this functionality multiple ways, for instance by using
Switch or Print's "active" handler, but Bypass is special because an inactive
Bypass has I<zero overhead>. Bypass modifies its neighboring elements so that
packets skip over the Bypass itself. In other words, the above configuration
behaves like one of the following two lines:

  ... source -> sink ...;
  ... source -> Print(Debug) -> sink ...;

Bypass can have two inputs and outputs. The additional push input (pull
output) reconnects the bypass path to the normal path, and makes it easier to
use Bypass in agnostic contexts. On a push Bypass, packets received on input
port 1 are forwarded to output port 0; on a pull Bypass, pull requests
received on output port 1 are forwarded to input port 0. The above
configuration could also be written this way:

  source :: TimedSource -> print_bypass :: Bypass -> sink ...;
  print_bypass [1] -> Print(Debug) -> [1] print_bypass;

And here is a pull version of Bypass in a related configuration:

  source :: Queue -> print_bypass :: Bypass -> sink ...;
  print_bypass [1] -> Print(Debug) -> [1] print_bypass;

Note that the bypass path is exactly the same in both cases.

Keyword arguments are:

=over 8

=item INLINE

Boolean. If true, then Bypass remains inline: it does not modify its
neighbors' ports, and it will introduce overhead. Default is false.

=back

=n

Bypass modifies Click internals at run time, which can be dangerous. Elements
that test their neighbors' classes should not be used next to
Bypass. (Elements should not test their neighbors' classes, however.) Bypass
should not be used in a devirtualized configuration unless its INLINE argument
is set to true.

=a

Switch, PullSwitch, Null, click-devirtualize
*/
class Bypass : public Element { public:

    Bypass() CLICK_COLD;

    const char *class_name() const	{ return "Bypass"; }
    const char *port_count() const	{ return "1-2/1-2"; }
    const char *flow_code() const	{ return "xy/[xy]x"; }
    void *cast(const char *name);

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void push(int port, Packet *p);
    Packet *pull(int port);

  private:
    struct Locator : public RouterVisitor {
        Element* _e;
        int _port;
        Locator(int from_port);
        bool visit(Element *e, bool isoutput, int port,
                   Element *from_e, int from_port, int distance);
    };

    struct Assigner : public RouterVisitor {
        Element* _e;
        int _port;
        Vector<int> _interesting;
        Assigner(Element *e, int port);
        bool visit(Element *e, bool isoutput, int port,
                   Element *from_e, int from_port, int distance);
    };

    bool _active;
    bool _inline;

    void fix();
    static int write_handler(const String &s, Element *e, void *user_data, ErrorHandler *errh) CLICK_COLD;
    friend struct Locator;
    friend struct Assigner;

};

CLICK_ENDDECLS
#endif
