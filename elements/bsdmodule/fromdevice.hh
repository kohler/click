#ifndef FROMDEVICE_HH
#define FROMDEVICE_HH

/*
=title FromDevice.b

=c

FromDevice(DEVNAME [, PROMISC, BURST, I<KEYWORDS>])

=s devices

reads packets from network device (kernel)

=d

This manual page describes the BSD kernel module version of the FromDevice
element. For the user-level element, read the FromDevice.u manual page.

Intercepts all packets received by the BSD network interface named DEVNAME
and pushes them out output 0. The packets include the link-level header.
DEVNAME may also be an Ethernet address, in which case FromDevice searches for
a device with that address.

FromDevice pulls packets from a per-interface queue in the context of the
Click kernel thread.  It emits at most BURST packets per scheduling;
BURST is 8 by default.  At interrupt time, the kernel queues packets
onto the per-interface queue if there is a FromDevice attached to that
interface.

If PROMISC is set (by default, it is not), then the device is put into
promiscuous mode while FromDevice is active.

Keyword arguments are:

=over 8

=item PROMISC

Boolean. Same as the PROMISC argument.

=item BURST

Unsigned integer. Same as the BURST argument.

=item ALLOW_NONEXISTENT

Allow nonexistent devices. If true, and no device named DEVNAME exists when
the router is initialized, then FromDevice will report a warning (rather than
an error). Later, while the router is running, if a device named DEVNAME
appears, FromDevice will seamlessly begin outputing its packets. Default is
false.

=back

=n

The BSD network stack (above the device layer) won't see any packets from
the device. If you want BSD to process packets, you should hand them to
ToBSD.

FromDevice accesses packets the same way BSD does: through interrupts.
This is bad for performance. If you care about performance and have a
polling-capable device, use PollDevice instead.

=a PollDevice, ToDevice, FromBSD, ToBSD, FromDevice.u */

#include "elements/bsdmodule/anydevice.hh"
#include "elements/standard/queue.hh"

class FromDevice : public AnyDevice, public Storage { public:
    
    FromDevice();
    ~FromDevice();

    const char *class_name() const	{ return "FromDevice"; }
    const char *processing() const	{ return PUSH; }
    FromDevice *clone() const		{ return new FromDevice; }
    void *cast(const char *);

    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();
    void add_handlers();
    void take_state(Element *, ErrorHandler *);

    void change_device(struct if_net *);
    
    /* process a packet. return 0 if not wanted after all. */
    int got_m(struct mbuf *);

    /* get some performance stats */
    int get_inq_drops();

    void run_scheduled();

    int _npackets;
#if CLICK_DEVICE_STATS
    int _perfcnt1_read, _perfcnt2_read;
    int _perfcnt1_push, _perfcnt2_push;
    long long _time_read, _time_push;
#endif

  private:

    bool _promisc;
    unsigned _burst;

    static const int QSIZE = 511;

};

#endif
