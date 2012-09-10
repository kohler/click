// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_CHANGEUID_HH
#define CLICK_CHANGEUID_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

ChangeUID()

=s control

relinquish root privilege

=d

Sets the current process's effective user and group IDs to its real user and
group IDs, respectively.  This relinquishes any set-uid-root privilege.

=n

ChangeUID's configure phase is set so that it runs immediately after the
"privileged" elements FromDevice.u and ToDevice.u, but before most other
elements.  Thus, this configuration:

   f1 :: FromDevice(eth0) -> ...
   f2 :: FromDump(/tmp/x) -> ...
   ChangeUID()

should fail to initialize if the user cannot read file F</tmp/x>.  However,
your mileage may vary.  Set-uid-root programs are a bad idea, and Click is no
exception.

*/

class ChangeUID : public Element { public:

    ChangeUID() CLICK_COLD;
    ~ChangeUID() CLICK_COLD;

    const char *class_name() const	{ return "ChangeUID"; }

    int configure_phase() const		{ return CONFIGURE_PHASE_PRIVILEGED+1; }
    int initialize(ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
