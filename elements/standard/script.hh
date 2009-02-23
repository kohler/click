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

In all cases, text arguments are subject to substitutions; see below.  Many
instructions come in two forms, as in C<set> and C<setq>, C<read> and
C<readq>, and C<write> and C<writeq>.  The non-C<q> forms perform
substitutions on the text, but do not remove any quotes from the result, while
the C<q> forms perform substitutions and then remove a layer of quoting.  For
example, assuming the 'c.count' read handler returns 0:

   set x $(c.count)
   print $x             => 0
   set x "$(c.count)"
   print $x             => "0"
   setq x "$(c.count)"
   print $x             => 0

=over 8

=item 'C<set> VAR TEXT', 'C<setq> VAR TEXT'

Sets the script variable $VAR to TEXT.

=item 'C<init> VAR TEXT', 'C<initq> VAR TEXT'

Initializes the script variable $VAR to TEXT.  The assignment happens exactly
once, when the Script element is initialized.  Later the instruction has no
effect.

=item 'C<print> [>FILE | >>FILE] [TEXT | HANDLER]'

Prints text, or the result of calling a read handler, followed by a newline.
At user level, the text is written to the standard output, except that if the
argument begins with > or >>, then the text is written or appended to the
specified FILE.  In the kernel, the text is written to the system log.

If C<print>'s argument starts with a letter, '@', or '_', then it is treated
as a read handler.  Otherwise, a layer of quotes is removed and the result is
printed.  For example, assuming the 'c.count' read handler returns "0":

   print c.count     => 0
   print "c.count"   => c.count
   print '"c.count"' => "c.count"
   set x c.count
   print $x          => c.count
   print $($x)       => 0

=item 'C<printn> [>FILE | >>FILE] [TEXT | HANDLER]'

Like C<print>, but does not append a newline.

=item 'C<read> HANDLER [ARGS]', 'C<readq> HANDLER [ARGS]'

Call a read handler and print the handler name and result to standard error.  (In the kernel, the result is printed to the system log.)  For example, the
configuration 'Idle -> c::Counter -> Idle; Script(read c.count)' would print
print this to standard error:

   c.count:
   0

Contrast the 'C<print>' instruction.

=item 'C<write> HANDLER [ARGS]', 'C<writeq> HANDLER [ARGS]'

Call a write handler.  The handler's return status is available in following
instructions as the '$?' variable.

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

Also, 'C<goto exit [CONDITION]>' and 'C<goto end [CONDITION]>' end execution
of the script, like 'C<exit>' and 'C<end>' respectively.  'C<goto begin
[CONDITION]>' transfers control to the first instruction, like 'C<loop>'.
'C<goto error [CONDITION]>' ends execution of the script with an error, like
'C<error>'.

=item 'C<loop>'

Transfers control to the first instruction.

=item 'C<exit>'

End execution of this script.

=item 'C<end>'

End execution of this script.  In signal scripts, 'C<end>' causes the script
to be reinstalled as a signal handler.

=item 'C<return> [VALUE]', 'C<returnq> [VALUE]'

End execution of this script.  For passive scripts, VALUE is returned as the
value of the C<run> handler.

=item 'C<error> [MSG]', 'C<errorq> [MSG]'

End execution of the script and indicate an error.  The optional error message
MSG is reported if given.

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

Within the script, the C<$args> variable equals the C<run> handler's
arguments.  C<$1>, C<$2>, etc. equal the first, second, etc. space-separated
portions of C<$args>, and C<$#> equals the number of space-separated
arguments.

=item C<PROXY>

A proxy script runs in response to I<any> handler (except Script's predefined
handlers).  Within the script, the C<$0> variable equals the handler's name.
Also, the C<$write> variable is "true" if the handler was called as a write
handler.  For example, consider:

   s :: Script(TYPE PROXY,
          goto nota $(ne $0 a),
	  returnq "you called 'a'",
	  label nota,
	  goto notb $(ne $0 b),
	  returnq "you called 'b'",
	  label notb,
	  error bad handler);

Calling the read handler "s.a" will return "you called 'a'", calling "s.b"
will return "you called 'b'", and anything else will produce a "bad handler"
error.

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

Compares two parameters and returns the result.  For example, 'C<eq 10 0xA>'
returns "C<true>", and 'C<le 9 8>' returns "C<false>".  If either parameter
cannot be interpreted as a number, performs a string comparison in bytewise
lexicographic order.  For example, 'C<eq 10x 10x>' return "C<true>".

=h not "read with parameters"

Useful for true/false operations.  Parses its parameter as a Boolean and
returns its negation.

=h and, or "read with parameters"

Useful for true/false operations.  Parses all parameters as Booleans and
returns their conjunction or disjunction, respectively.

=h if "read with parameters"

Expects three space-separated parameters, the first a Boolean.  Returns the
second parameter if the Boolean is true, or the third parameter if the Boolean
is false.

=h sprintf "read with parameters"

Parses its parameters as a space-separated list of arguments.  The first
argument is a format string; the remaining arguments are formatted
accordingly.  For example, 'C<sprintf "%05x" 127>' returns "C<0007F>".

=h random "read with parameters"

Given zero arguments, returns a random integer between 0 and RAND_MAX.  Given
one argument N, returns a random integer between 0 and N-1.  Given two
arguments N1 and N2, returns a random integer between N1 and N2.

=h now r

Returns the current timestamp.

=h cat "read with parameters"

User-level only.  Argument is a filename; reads and returns the file's
contents.  This handler is "private," meaning that it is not accessible via
ControlSocket.

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
	INSN_PRINT, INSN_PRINTN, INSN_READ, INSN_READQ, INSN_WRITE, INSN_WRITEQ,
	INSN_SET, insn_setq, INSN_INIT, insn_initq, INSN_SAVE, INSN_APPEND,
	INSN_STOP, INSN_END, INSN_EXIT, INSN_LABEL, INSN_GOTO, INSN_RETURN,
	insn_returnq, insn_error, insn_errorq,
	INSN_WAIT_PSEUDO, INSN_LOOP_PSEUDO
    };

  private:

    enum Type {
	TYPE_ACTIVE, TYPE_DRIVER, TYPE_SIGNAL, TYPE_PASSIVE, type_proxy
    };

    enum {
	max_jumps = 1000, STEP_NORMAL = 0, STEP_ROUTER, STEP_TIMER, STEP_JUMP,
	LABEL_EXIT = -1, LABEL_END = -2, label_error = -3, LABEL_BEGIN = 0
    };

    Vector<int> _insns;
    Vector<int> _args;
    Vector<int> _args2;
    Vector<String> _args3;

    Vector<String> _vars;
    String _run_handler_name;
    String _run_args;
    int _run_op;

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
	bool expand(const String &, int vartype, int quote, StringAccum &) const;
    };

    enum {
	ST_STEP = 0, ST_RUN, ST_GOTO,
	AR_ADD = 0, AR_SUB, AR_MUL, AR_DIV, AR_IDIV,
	AR_LT, AR_EQ, AR_GT, AR_GE, AR_NE, AR_LE, // order is important
	AR_FIRST, AR_NOT, AR_SPRINTF, ar_random, ar_cat,
	ar_and, ar_or, ar_now, ar_if
    };

    void add_insn(int, int, int = 0, const String & = String());
    int step(int nsteps, int step_type, int njumps, ErrorHandler *errh);
    int complete_step(String *retval);
    int find_label(const String &) const;
    int find_variable(const String &) const;

    static int step_handler(int, String&, Element*, const Handler*, ErrorHandler*);
    static int arithmetic_handler(int, String&, Element*, const Handler*, ErrorHandler*);
    static int star_write_handler(const String&, Element*, void*, ErrorHandler*);

    friend class DriverManager;
    friend class Expander;

};

CLICK_ENDDECLS
#endif
