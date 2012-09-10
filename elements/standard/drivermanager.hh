// -*- c-basic-offset: 4; related-file-name: "../../../elements/standard/drivermanager.cc" -*-
#ifndef CLICK_DRIVERMANAGER_HH
#define CLICK_DRIVERMANAGER_HH
#include "script.hh"
CLICK_DECLS

/*
=c

DriverManager(INSTRUCTIONS...)

=s control

a Script that manages driver stop events

=d

DriverManager is simply a Script element whose default TYPE is "C<DRIVER>".

Click I<driver stop events> suggest that the driver should stop processing.
Any element can register a driver stop event; for instance, trace processing
elements can stop the driver when they finish a trace file.  You generally
request this functionality by supplying a 'STOP true' keyword argument.

Driver stop events normally stop the driver: the user-level driver calls
C<exit(0)>, or the kernel driver kills the relevant kernel threads.  The
DriverManager element changes this behavior.  When a driver stop event occurs,
the router steps through the DriverManager's script by calling its C<step>
handler.  The driver exits only when the script ends or a C<stop> instruction
is executed.

For example, the following DriverManager element ensures that an element,
C<k>, has time to clean itself up before the driver is stopped. It waits for
the first driver stop event, then calls C<k>'s C<cleanup> handler, waits for a
tenth of a second, and stops the driver.

  DriverManager(pause, write k.cleanup, wait 0.1s, stop);

Use this idiom when one of your elements must emit a last packet or two before
the router configuration is destroyed.

=a Script

*/

class DriverManager : public Script { public:

    DriverManager() CLICK_COLD;

    const char *class_name() const	{ return "DriverManager"; }

};

/* XXX=n

The driver's step instructions have a special property: If the driver stops
while the script is paused at a C<wait> instruction, then the script will make
I<two> steps, rather than one.  To understand this difference, consider the
following two scripts:

  (1)  s::Script(wait 1s, print "before", pause, print "after");
       Script(wait 0.5s, write s.step)

  (2)  DriverManager(wait 1s, print "before", pause, print "after");
       Script(wait 0.5s, write stop)

The first script will wait 0.5s, print "before" (because the second script has
stepped the first script past its C<wait> instruction), then block at the
C<pause> instruction.  The second script will wait 0.5s, then print "before",

Do not rely on this functionality, since we may change it soon.

*/

CLICK_ENDDECLS
#endif
