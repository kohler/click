// -*- c-basic-offset: 4 -*-
#ifndef CLICK_FROMDEVICE_HH
#define CLICK_FROMDEVICE_HH

/*
=c

FromDevice(DEVNAME [, PROMISC, BURST, I<KEYWORDS>])

=s netdevices

reads packets from network device (Linux kernel)

=d

This manual page describes the Linux kernel module version of the FromDevice
element. For the user-level element, read the FromDevice.u manual page.

Intercepts all packets received by the Linux network interface named DEVNAME
and pushes them out output 0. The packets include the link-level header.
DEVNAME may also be an Ethernet address, in which case FromDevice searches for
a device with that address.

FromDevice receives packets at interrupt time. As this happens, FromDevice
simply stores the packets in an internal queue. Later, in the Click kernel
thread -- that is, not at interrupt time -- FromDevice emits packets from that
queue as it is scheduled by the driver. It emits at most BURST packets per
scheduling; BURST is 8 by default.

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

Linux won't see any packets from the device. If you want Linux to process
packets, you should hand them to ToHost.

FromDevice accesses packets the same way Linux does: through interrupts.
This is bad for performance. If you care about performance and have a
polling-capable device, use PollDevice instead.

Linux device drivers, and thus FromDevice, should set packets' timestamp,
packet-type, and device annotations.

=a PollDevice, ToDevice, FromHost, ToHost, FromDevice.u */

#include "elements/linuxmodule/anydevice.hh"
#include <click/standard/storage.hh>

class FromDevice : public AnyTaskDevice, public Storage { public:
    
    FromDevice();
    ~FromDevice();

    static void static_initialize();
    static void static_cleanup();

    const char *class_name() const	{ return "FromDevice"; }
    const char *port_count() const	{ return PORTS_0_1; }
    const char *processing() const	{ return PUSH; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();
    void take_state(Element *, ErrorHandler *);

    /* process a packet. return 0 if not wanted after all. */
    int got_skb(struct sk_buff *);

    bool run_task(Task *);

    void reset_counts();

    unsigned drops()                   { return _drops; }
    unsigned runs()                    { return _runs; }
    unsigned empty_runs()              { return _empty_runs; }
    unsigned pushes()                  { return _pushes; }

  private:

    unsigned _burst;
    unsigned _drops;

    unsigned _runs;
    unsigned _empty_runs;
    unsigned _pushes;

    enum { QSIZE = 511 };
    Packet *_queue[QSIZE+1];
#if CLICK_DEBUG_SCHEDULING
    struct Schinfo {
	struct timeval enq_time;
	char enq_state;
	char enq_woke_process;
	char enq_task_scheduled;
	uint32_t enq_epoch;
	uint32_t enq_task_epoch;
    };
    Schinfo _schinfo[QSIZE+1];
    void emission_report(int);
#endif

};

#endif
