#ifndef PEEKHANDLERS_HH
#define PEEKHANDLERS_HH
#include <click/element.hh>
#include <click/timer.hh>

/*
=c

PeekHandlers([DELAY or HANDLER] ...)

=s debugging

calls read handlers at specified times

=io

None

=d

Runs read handlers automatically at specified times. Each configuration
argument should be either `DELAY', a delay in seconds; `ELEMENT.HANDLERNAME',
a directive to call ELEMENT's HANDLERNAME read handler and report the result;
`quit', a directive to stop the driver; or `loop', a directive to start over
from the first PeekHandlers argument. PeekHandlers processes its arguments in
order, reading handlers as they appear. A `DELAY' directive causes it to wait
for DELAY seconds before continuing.

At user level, the results of any read handlers are printed to standard
error. In the Linux kernel module, they are printed to /var/log/messages
(accessible through dmesg(1)) and to /proc/click/errors.

=e

  PeekHandlers(q.length,
               1.5, // delay for 1.5 seconds
               q.length);

=a

PokeHandlers, dmesg(1) */

class PeekHandlers : public Element { public:

  PeekHandlers();
  ~PeekHandlers();

  const char *class_name() const		{ return "PeekHandlers"; }
  PeekHandlers *clone() const			{ return new PeekHandlers; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

 private:

  enum { HID_WAIT = -1, HID_LOOP = -2, HID_QUIT = -3 };
  
  int _pos;
  Vector<Element *> _h_element;
  Vector<int> _h_hid;
  Vector<int> _h_extra;
  Timer _timer;

  void push_command(Element *, int hid, int extra);
  int do_configure(const Vector<String> &, ErrorHandler *);
  static void timer_hook(Timer *, void *);
  
};

#endif
