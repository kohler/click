// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_POKEHANDLERS_HH
#define CLICK_POKEHANDLERS_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

PokeHandlers(DIRECTIVE, ...)

=s debugging

calls write handlers at specified times

=io

None

=d

Runs read and write handlers at specified times. Each configuration argument
is a directive, taken from this list:

=over 8

=item `read HANDLER'

Call a read handler and report its result. At user level, the result is
printed on standard error; in a Linux kernel, it is printed to
/var/log/messages. HANDLER should be either a global read handler name, or
`ELEMENT.HNAME', where ELEMENT is an element name and HNAME the name of one of
its read handlers.

=item `write HANDLER [VALUE]'

Call a write handler with a given value. At user level, errors reported by
write handlers are printed to standard error. In the Linux kernel module, they
are printed to /var/log/messages (accessible through dmesg(1)) and to
/proc/click/errors.

=item `stop'

Stop the driver.

=item `wait DELAY'

Wait for DELAY seconds before continuing to the next directive.

=item `pause'

Wait until the `unpause' write handler is called, then continue to the next directive.

=item `loop'

Start over from the first directive.

=back

=e

  PokeHandlers(write red.max_p 0.8,
               wait 1.5, // delay for 1.5 seconds
               write red.max_p 0.5);

=h unpause write-only
If paused, continue to next directive.  Otherwise, there is no effect.

=h paused read-only
Returns `true' if paused, else `false'.

=a dmesg(1) */

class PokeHandlers : public Element { public:

    PokeHandlers();
    ~PokeHandlers();

    const char *class_name() const		{ return "PokeHandlers"; }
    PokeHandlers *clone() const			{ return new PokeHandlers; }
    
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    bool can_live_reconfigure() const		{ return true; }

    void add_handlers();

  private:

    static Element * const STOP_MARKER;
    static Element * const LOOP_MARKER;
    static Element * const PAUSE_MARKER;

    int _pos;
    bool _paused;
    Vector<Element *> _h_element;
    Vector<String> _h_handler;
    Vector<String> _h_value;
  
    Vector<int> _h_timeout;
    Timer _timer;

    static void timer_hook(Timer *, void *);
    void add(Element *, const String &, const String &, int);
    void unpause();

    static String read_param(Element *, void *);
    static int write_param(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
