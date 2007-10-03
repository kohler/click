// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SCRIPT_HH
#define CLICK_SCRIPT_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/variableenv.hh>
CLICK_DECLS

/*
=c

Script(INSTRUCTIONS...)

=s control

script a Click router configuration

=d

The Script element implements a simple scripting language useful for
controlling Click configurations.  Scripts can set variables, call handlers,
wait for prodding from other elements, and stop the router.

=head1 INSTRUCTIONS

Each configuration argument is an I<instruction> (except for optional
keywords; see below).  Script generally processes these instructions
sequentially.

=head2 Handler Instructions

In all cases, text arguments are subject to substitutions; see below.

=over 8

=item 'C<set> VAR TEXT'

Sets the script variable $VAR to TEXT.

=item 'C<init> VAR TEXT'

Initializes the script variable $VAR to TEXT.  This assignment happens exactly
once, when the Script element is initialized; later the instruction has no
effect.

=item 'C<print> [>FILE | >>FILE] [TEXT | HANDLER]'

Prints text, or the result of calling a read handler, followed by a newline.
At user level, the text is written to the standard output, except that if the
argument begins with > or >>, then the text is written or appended to the
specified FILE.  In the kernel, the text is written to the system log.

If C<print>'s argument starts with a letter, '@', or '_', then it is treated
as a read handler.  Otherwise, it is treated as quoted text; Script prints the
unquoted version.  For example, assuming the 'c.count' read handler
returns "0":

   print c.count     => 0
   print "c.count"   => c.count
   print '"c.count"' => "c.count"
   set x c.count
   print $x          => c.count
   print $($x)       => 0

=item 'C<read> HANDLER [ARG...]'

Call a read handler and print the handler name and result to standard error.  (In the kernel, the result is printed to the system log.)  For example, the
configuration 'Idle -> c::Counter -> Idle; Script(read c.count)' would print
print this to standard error:

   c.count:
   0

Contrast the 'C<print>' instruction.

=item 'C<write> HANDLER [ARG...]'

Call a write handler.  The handler's return status is available in following
instructions as the '$?' variable.

=item 'C<readq> HANDLER [ARG...]', 'C<writeq> HANDLER [ARG...]'

Same as C<read> and C<write>, but removes one layer of quoting from the ARGs
before calling the handler.

=back

=head2 Blocking Instructions

=over 8

=item 'C<pause> [COUNT]'

Block until the Script element's 'step' handler is called COUNT times.  COUNT
defaults to 1.

=item 'C<wait> TIME'

Wait for TIME seconds, or until a step, whichever comes first; then go to the
next instruction.  TIME has microsecond precision.

=back

=head2 Control Instructions

=over 8

=item 'C<label> LABEL'

Defines a label named LABEL.

=item 'C<goto> LABEL [CONDITION]'

Transfers control to the named label.  Script elements detect loops; if an
element's script appears to be looping (it executes 1000 goto instructions
without blocking), the script is disabled.  If CONDITION is supplied, then the
branch executes only when CONDITION is true.

Also, 'C<goto exit [CONDITION]>' and 'C<goto end [CONDITION]>'
end execution of the script, like 'C<exit>' and 'C<end>' respectively, and
'C<goto begin [CONDITION]>' transfers control to the first instruction, like
'C<loop>'.

=item 'C<loop>'

Transfers control to the first instruction.

=item 'C<return> [VALUE]'

End execution of this script, returning VALUE.  This instruction is most
useful for passive scripts; VALUE will be returned as the value of the C<run>
handler.

=item 'C<exit>', 'C<end>'

End execution of this script.  For signal scripts, the 'C<exit>' instruction
I<does not> reinstall the script, whereas the 'C<end>' instruction does.

=back

=head1 SCRIPT TYPES

Scripts come in several types, including active scripts, which start running
as soon as the configuration is loaded; passive scripts, which run only when
prodded; signal scripts, which run in response to a signal; and driver
scripts, which are active scripts that also control when the driver stops.
The optional TYPE keyword argument is used to select a script type.  The types
are:

=over 8

=item C<ACTIVE>

An active script starts running as soon as the router is initialized.  This is
the default.

=item C<PASSIVE>

A passive script runs in response to a handler, namely the C<run> handler.
Passive scripts can help build complex handlers from existing simple ones; for
example, here's a passive script whose C<s.run> read handler returns the sum
of two Counter handlers.

   ... c1 :: Counter ... c2 :: Counter ...
   s :: Script(TYPE PASSIVE,
          return $(add $(c1.count) $(c2.count)))

Within the script, the C<run> handler's arguments, if any, are available
via the C<$args> variable.  The first, second, and so forth space-separated
portions of C<$args> are available via the C<$1>, C<$2>, ... variables.
	  
=item C<DRIVER>

A driver script manages the Click driver's stop events.  See DriverManager for
more information.

=item C<SIGNAL> SIGNO...

User-level only: A signal script runs in response to the signal(s) specified
by the SIGNO argument(s).  Each SIGNO can be an integer or a signal name, such
as INT or HUP.  Soon after the driver receives a named signal, this script
will run.  The signal handler is automatically blocked until the script runs.
The signal script will be reinstalled atomically as long as the script
completes without blocking.  If it blocks, however, the signal script will not
be installed from the blocking point until the script completes.  If multiple
Script elements select the same signal, all the scripts will run.

=back

=head1 SUBSTITUTIONS

Text in most Script instructions undergoes variable substitution.  References
to script variables, such as 'C<$x>', are replaced by the variable text.
Additionally, the form 'C<$(HANDLER [ARG...])>' can be used to interpolate a
read handler's value.  Variable and handler references can be nested inside
a 'C<$(...)>' block.  For example, the following script will print 0, 1, 2, 3,
and 4 on separate lines, then exit.  Note the use of Script's arithmetic
handlers.

   s :: Script(set x 0,
               label begin_loop,
	       print $x,
	       set x $(s.add $x 1),
	       goto begin_loop $(s.lt $x 5),
	       stop);

This can be further shortened since local handler references do not require
the element name.  Thus, "$(s.add ...)" can be written "$(add ...)", as below.

   Script(set x 0,
          label begin_loop,
	  print $x,
	  set x $(add $x 1),
	  goto begin_loop $(lt $x 5),
	  stop);

=h step write-only

Advance the instruction pointer past the current blocking instruction (C<pause> or C<wait>).  A numeric argument will step past that many blocking instructions.

=h goto write-only

Move the instruction pointer to the specified label.

=h run read/write

Run the script.  If the script ends with a 'C<return>' instruction, then the
handler returns with that value.

=h add "read with parameters"

Useful for arithmetic.  Adds a space-separated list of integers; for example,
'C<add 10 5 2>' returns "C<17>".  (At user level, the arithmetic and
comparison operators can parse floating-point numbers as well as integers.)

=h sub "read with parameters"

Subtracts a space-separated list of
numbers; for example, 'C<sub 10 5 2>' returns
"C<3>".

=h mul, div, idiv "read with parameters"

Multiplies or divides a space-separated list of numbers and returns the
result.  At user level, the 'C<idiv>' handler truncates its result to an
integer and returns that, whereas the 'C<div>' handler returns a
floating-point number; in the kernel, 'C<idiv>' and 'C<div>' both perform
integer division.

=h eq, ne, lt, gt, le, ge "read with parameters"

Compares two parameters and return the result.  For example, 'C<eq 10
0xA>' returns "C<true>", but 'C<le 9 8>' returns "C<false>".  If either
parameter cannot be interpreted as a number, performs a string comparison.
For example, 'C<eq 10x 10x>' return "C<true>".

=h not "read with parameters"

Useful for true/false operations.  Parses its parameter as a Boolean and
returns its negation.

=h sprintf "read with parameters"

Parses its parameters as a space-separated list of arguments.  The first
argument is a format string; the remaining arguments are formatted
accordingly.  For example, 'C<sprintf "%05x" 127>' returns "C<0007F>".

=a DriverManager

*/

class Script : public Element { public:

    Script();
    ~Script();

    static void static_initialize();
    static void static_cleanup();

    const char *class_name() const	{ return "Script"; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();

    void run_timer(Timer *);

    enum Insn {
	INSN_INITIAL, INSN_WAIT_STEP, INSN_WAIT_TIME, // order required
	INSN_PRINT, INSN_READ, INSN_READQ, INSN_WRITE, INSN_WRITEQ, INSN_SET,
	INSN_INIT, INSN_SAVE, INSN_APPEND, INSN_STOP, INSN_END, INSN_EXIT,
	INSN_LABEL, INSN_GOTO, INSN_RETURN,
	INSN_WAIT_PSEUDO, INSN_LOOP_PSEUDO
    };

  private:

    enum Type {
	TYPE_ACTIVE, TYPE_DRIVER, TYPE_SIGNAL, TYPE_PASSIVE
    };

    enum {
	MAX_JUMPS = 1000, STEP_NORMAL = 0, STEP_ROUTER, STEP_TIMER, STEP_JUMP,
	LABEL_EXIT = -1, LABEL_END = -2, LABEL_BEGIN = 0
    };

    Vector<int> _insns;
    Vector<int> _args;
    Vector<int> _args2;
    Vector<String> _args3;
    
    Vector<String> _vars;
    String _run_args;

    int _insn_pos;
    int _step_count;
    int _type;
    int _write_status;
#if CLICK_USERLEVEL
    Vector<int> _signos;
#endif

    Timer _timer;
    int *_cur_steps;

    struct Expander : public VariableExpander {
	Script *script;
	ErrorHandler *errh;
	bool expand(const String &, int vartype, int quote, StringAccum &);
    };
    
    void add_insn(int, int, int = 0, const String & = String());
    int step(int nsteps, int step_type, int njumps);
    int find_label(const String &) const;
    int find_variable(const String &) const;

    static int step_handler(int, String&, Element*, const Handler*, ErrorHandler*);
    static int arithmetic_handler(int, String&, Element*, const Handler*, ErrorHandler*);

    friend class DriverManager;
    friend class Expander;

};

CLICK_ENDDECLS
#endif
