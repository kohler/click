#ifndef DRIVERMANAGER_HH
#define DRIVERMANAGER_HH
#include <click/element.hh>
#include <click/timer.hh>

/*
=c

DriverManager(INSTRUCTIONS...)

=s information

manages driver stop events

=io

None

=d

A router configuration may use a DriverManager element to gain control over
when the Click driver should stop.

Any Click element may request a I<driver pause>. Normally, the first pause
encountered stops the Click driver---that is, the user-level driver calls
C<exit(0)>, or the kernel driver kills the relevant kernel threads. In element
documentation, therefore, a driver pause is usually called ``stopping the
driver'', and the user asks an element to request a driver pause by supplying
a `STOP true' keyword argument.

The DriverManager element changes this behavior. When the driver detects a
pause, it asks DriverManager what to do. Depending on its arguments,
DriverManager will tell the driver to stop immediately, to wait a while, or to
continue until the next pause, possibly after calling write handlers on other
elements.

Each configuration argument is an I<instruction>; DriverManager processes
these instructions sequentially. Instructions include:

=over 8

=item `C<stop>'

Stop the driver.

=item `C<wait> TIME'

Wait for TIME seconds, or until a driver pause, whichever comes first; then go
to the next instruction.

=item `C<wait_pause> [COUNT]'

Wait for COUNT driver pauses, then go to the next instruction. COUNT defaults
to one. You may say `C<wait_stop>' instead of `C<wait_pause>'.

=item `C<call> ELEMENT.HANDLER [DATA]'

Call ELEMENT's write handler named HANDLER, passing it the string DATA; then
go to the next instruction. DATA defaults to the empty string.

=item `C<print> ELEMENT.HANDLER'

Call ELEMENT's read handler named HANDLER and print the result.

=back

DriverManager adds an implicit `C<stop>' instruction to the end of its
instruction list. As a special case, `C<DriverManager()>', with no arguments,
is equivalent to `C<DriverManager(wait_pause, stop)>'.

A router configuration can contain at most one DriverManager element.

=e

The following DriverManager element ensures that an element, C<k>, has time to
clean itself up before the driver is stopped. It waits for the first driver
pause, then calls C<k>'s C<cleanup> handler, waits for a tenth of a second,
and stops the driver.

  DriverManager(wait_pause, call k.cleanup, wait 0.1, stop);

Use this idiom when one of your elements must emit a last packet or two before
the router configuration is destroyed.

*/

class DriverManager : public Element { public:
  
  DriverManager();
  ~DriverManager();
  
  const char *class_name() const	{ return "DriverManager"; }
  DriverManager *clone() const		{ return new DriverManager; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void run_scheduled();
  void handle_stopped_driver();
  
 private:

  enum Insn { INSN_WAIT_STOP, INSN_WAIT, INSN_STOP, INSN_CALL, INSN_PRINT };
  
  Vector<int> _insns;
  Vector<int> _args;
  Vector<int> _args2;
  Vector<String> _args3;

  int _insn_pos;
  int _insn_arg;

  Timer _timer;

  void add_insn(int, int, const String & = String());
  bool step_insn();
  
};

#endif
