// -*- c-basic-offset: 4; related-file-name: "../../../elements/standard/drivermanager.cc" -*-
#ifndef CLICK_DRIVERMANAGER_HH
#define CLICK_DRIVERMANAGER_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

DriverManager(INSTRUCTIONS...)

=s information

manages driver stop events

=io

None

=d

The DriverManager element gives you control over when the Click driver should
stop.

Click I<driver stop events> suggest that the driver should stop processing.
Any element can register a driver stop event; for instance, trace processing
elements can stop the driver when they finish a trace file.  You generally
request this functionality by supplying a 'STOP true' keyword argument.

Driver stop events normally stop the driver: the user-level driver calls
C<exit(0)>, or the kernel driver kills the relevant kernel threads.  The
DriverManager element changes this behavior.  When a driver stop event occurs,
the router asks DriverManager what to do next.  Depending on its arguments,
DriverManager will tell the driver to stop immediately, to wait a while, or to
continue until the next driver stop event, possibly after calling handlers on
other elements.

Each configuration argument is an I<instruction>; DriverManager processes
these instructions sequentially. Instructions include:

=over 8

=item 'C<stop>'

Stop the driver.

=item 'C<wait>'

Consume a driver stop event, then go to the next instruction.

=item 'C<wait_time> TIME'

Wait for TIME seconds, or until a driver stop event occurs, whichever comes
first; then go to the next instruction.  Any driver stop is not consumed.

=item 'C<wait_stop> [COUNT]'

Consume COUNT driver stop events, then go to the next instruction.  COUNT
defaults to one.

=item 'C<read> ELEMENT.HANDLER'

Call ELEMENT's read handler named HANDLER and print the result.

=item 'C<write> ELEMENT.HANDLER [DATA]'

Call ELEMENT's write handler named HANDLER, passing it the string DATA; then
go to the next instruction. DATA defaults to the empty string.

=item 'C<write_skip> ELEMENT.HANDLER [DATA]'

Same as 'C<write>', except that this directive is skipped when there is
another driver stop event pending.

=back

The user level driver supports two additional instructions:

=over 8

=item 'C<save> ELEMENT.HANDLER FILE'

Call ELEMENT's read handler named HANDLER and save the result to FILE.  If
FILE is 'C<->', writes the handler value to the standard output.

=item 'C<append> ELEMENT.HANDLER FILE'

Call ELEMENT's read handler named HANDLER and append the result to FILE.  If
FILE is 'C<->', writes the handler value to the standard output.

=back

DriverManager adds an implicit 'C<stop>' instruction to the end of its
instruction list. As a special case, 'C<DriverManager()>', with no arguments,
is equivalent to 'C<DriverManager(wait_stop, stop)>'.

DriverManager accepts the following keyword argument:

=over 8

=item CHECK_HANDLERS

Boolean. If false, then DriverManager will ignore bad handler names, rather
than failing to initialize. Default is true.

=back

A router configuration can contain at most one DriverManager element.

=e

The following DriverManager element ensures that an element, C<k>, has time to
clean itself up before the driver is stopped. It waits for the first driver
stop event, then calls C<k>'s C<cleanup> handler, waits for a tenth of a
second, and stops the driver.

  DriverManager(wait_stop, write k.cleanup, wait_time 0.1, stop);

Use this idiom when one of your elements must emit a last packet or two before
the router configuration is destroyed.

*/

class DriverManager : public Element { public:

    DriverManager();
    ~DriverManager();

    const char *class_name() const	{ return "DriverManager"; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    void run_timer();
    bool handle_stopped_driver();

    int stopped_count() const		{ return _stopped_count; }

  private:

    enum Insn { INSN_INITIAL, INSN_WAIT_STOP, INSN_WAIT_TIME, // order required
		INSN_READ, INSN_WRITE, INSN_WRITE_SKIP, INSN_SAVE, INSN_APPEND,
		INSN_IGNORE, INSN_STOP };

    Vector<int> _insns;
    Vector<int> _args;
    Vector<int> _args2;
    Vector<String> _args3;

    int _insn_pos;
    int _stopped_count;
    bool _check_handlers;

    Timer _timer;

    void add_insn(int, int, const String & = String());
    bool step_insn();

};

CLICK_ENDDECLS
#endif
