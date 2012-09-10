#ifndef CLICK_PULLSWITCH_HH
#define CLICK_PULLSWITCH_HH
#include "elements/simple/simplepullswitch.hh"
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

PullSwitch([INPUT])

=s scheduling

forwards pull requests to settable input

=d

On every pull, PullSwitch returns the packet pulled from one of its input
ports -- specifically, INPUT. The default INPUT is zero; negative INPUTs
mean always return a null packet. You can change INPUT with a write handler.
PullSwitch has an unlimited number of inputs.

PullSwitch supports notification, unlike SimplePullSwitch.  An element
downstream of PullSwitch will sleep when PullSwitch's active input is
dormant.  (In contrast, with SimplePullSwitch, a downstream element will
sleep only when I<all> inputs are dormant.)

=h switch read/write

Return or set the K parameter.

=h CLICK_LLRPC_GET_SWITCH llrpc

Argument is a pointer to an integer, in which the Switch's K parameter is
stored.

=h CLICK_LLRPC_SET_SWITCH llrpc

Argument is a pointer to an integer. Sets the K parameter to that integer.

=a SimplePullSwitch, StaticPullSwitch, PrioSched, RoundRobinSched,
StrideSched, Switch */

class PullSwitch : public SimplePullSwitch { public:

    PullSwitch() CLICK_COLD;

    const char *class_name() const		{ return "PullSwitch"; }
    void *cast(const char *name);

    int initialize(ErrorHandler *errh) CLICK_COLD;
    void cleanup(CleanupStage stage) CLICK_COLD;

    void set_input(int input);

    Packet *pull(int);

  protected:

    ActiveNotifier _notifier;
    NotifierSignal *_signals;

    static void wake_callback(void *, Notifier *);

};

CLICK_ENDDECLS
#endif
