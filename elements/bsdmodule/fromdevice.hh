#ifndef CLICK_FROMDEVICE_BSDMODULE_HH
#define CLICK_FROMDEVICE_BSDMODULE_HH

/*
=title FromDevice.b

=c

FromDevice(DEVNAME [, PROMISC, BURST, I<KEYWORDS>])

=s netdevices

reads packets from network device (BSD kernel)

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
Performance is system-dependent (FreeBSD has a native polling mode
which can be used for better performance and stability).

=a ToDevice, FromHost, ToHost, FromDevice.u */

#include <click/element.hh>
#include "elements/bsdmodule/anydevice.hh"
#include <click/standard/storage.hh>
CLICK_DECLS

#define	CLICK_IFP2FD(ifp)	(IFP2AC(ifp)->ac_netgraph)

class FromDevice : public AnyDevice, public Storage { public:

    FromDevice() CLICK_COLD;
    ~FromDevice() CLICK_COLD;

    const char *class_name() const	{ return "FromDevice"; }
    const char *port_count() const	{ return PORTS_0_1; }
    const char *processing() const	{ return PUSH; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void change_device(struct if_net *);

    int get_inq_drops();	// get some performance stats

    bool run_task(Task *);

    //poll_handler_t *_poll_handler;

    int _npackets;
#if CLICK_DEVICE_STATS
    int _perfcnt1_read, _perfcnt2_read;
    int _perfcnt1_push, _perfcnt2_push;
    long long _time_read, _time_push;
#endif

    unsigned _readers;		// how many readers registered for this?

    struct ifqueue *_inq;
    int _polling;
    int _poll_status_tick;
    uint64_t _tstamp;

  private:

    bool _promisc;
    unsigned _burst;

    static const int QSIZE = 511;

};

CLICK_ENDDECLS
#endif
